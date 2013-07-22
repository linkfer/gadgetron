#include "GadgetIsmrmrdReadWrite.h"
#include "IsmrmrdDumpGadget.h"
#include "Gadgetron.h"
namespace Gadgetron{


  std::string get_date_time_string()
  {
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    
    std::stringstream str;
    str << timeinfo->tm_year+1900
	<< std::setw(2) << std::setfill('0') << timeinfo->tm_mon+1
	<< std::setw(2) << std::setfill('0') << timeinfo->tm_mday
	<< "-"
	<< std::setw(2) << std::setfill('0') << timeinfo->tm_hour
	<< std::setw(2) << std::setfill('0') << timeinfo->tm_min
	<< std::setw(2) << std::setfill('0') << timeinfo->tm_sec;
    
    std::string ret = str.str();
    
    return ret;
  }

  IsmrmrdDumpGadget::IsmrmrdDumpGadget()
    : Gadget2()
    , file_prefix_("ISMRMRD_DUMP")
    , ismrmrd_file_name_("ISMRMRD_DUMP.h5") //This will be reset during configuration
    , append_timestamp_(true)
  {
    set_parameter("file_prefix","ISMRMRD_DUMP",false);
    set_parameter("append_timestamp","1",false);

  }

int IsmrmrdDumpGadget
::process(GadgetContainerMessage<ISMRMRD::AcquisitionHeader>* m1,
	  GadgetContainerMessage< hoNDArray< std::complex<float> > >* m2)
{


  ISMRMRD::Acquisition ismrmrd_acq;

  ismrmrd_acq.setHead(*m1->getObjectPtr());
  
  std::valarray<float> d(reinterpret_cast<float*>(m2->getObjectPtr()->get_data_ptr()),
			 m2->getObjectPtr()->get_number_of_elements()*2);

  ismrmrd_acq.setData(d);

  if (m2->cont()) {
    //Write trajectory
    if (ismrmrd_acq.getTrajectoryDimensions() == 0) {
      GADGET_DEBUG1("Malformed dataset. Trajectory attached but trajectory dimensions == 0\n");
      return GADGET_FAIL;
    }
    
    GadgetContainerMessage< hoNDArray<float> >* m3 = AsContainerMessage< hoNDArray<float> >(m2->cont());

    if (!m3) {
      GADGET_DEBUG1("Error casting trajectory data package");
      return GADGET_FAIL;
    } 

    std::valarray<float> t(m3->getObjectPtr()->get_data_ptr(),
			   m3->getObjectPtr()->get_number_of_elements());
    
    ismrmrd_acq.setTraj(t);

  } else {
    if (ismrmrd_acq.getTrajectoryDimensions() != 0) {
      GADGET_DEBUG1("Malformed dataset. Trajectory dimensions not zero but no trajectory attached\n");
      return GADGET_FAIL;
    }
  }


  {
    ISMRMRD::HDF5Exclusive lock;
    if (ismrmrd_dataset_->appendAcquisition(&ismrmrd_acq) < 0) {
      GADGET_DEBUG1("Error appending ISMRMRD Dataset\n");
      return GADGET_FAIL;
    }
  }


  //It is enough to put the first one, since they are linked
  if (this->next()->putq(m1) == -1) {
    m1->release();
    ACE_ERROR_RETURN( (LM_ERROR,
		       ACE_TEXT("%p\n"),
		       ACE_TEXT("IsmrmrdDumpGadget::process, passing data on to next gadget")),
		      -1);
  }

  return 0;
}

int IsmrmrdDumpGadget
::process_config(ACE_Message_Block* mb)
{

  file_prefix_ = *(get_string_value("file_prefix").get());
  append_timestamp_ = (get_int_value("append_timestamp") > 0);

  //Generate filename
  if (append_timestamp_) {
    ismrmrd_file_name_ = file_prefix_ + std::string("_") + get_date_time_string() + std::string(".h5");
  } else {
    ismrmrd_file_name_ = file_prefix_ + std::string(".h5");
  }
  
  
  ISMRMRD::HDF5Exclusive lock; //This will ensure threadsafe access to HDF5
  ismrmrd_dataset_ = boost::shared_ptr<ISMRMRD::IsmrmrdDataset>(new ISMRMRD::IsmrmrdDataset(ismrmrd_file_name_.c_str(), "dataset"));
 
  std::string xml_config(mb->rd_ptr());

  if (ismrmrd_dataset_->writeHeader(xml_config) < 0 ) {
    GADGET_DEBUG1("Failed to write XML header to HDF file\n");
    return GADGET_FAIL;
  }
 
  return GADGET_OK;
}

GADGET_FACTORY_DECLARE(IsmrmrdDumpGadget)
}

