#include <glog/logging.h>
#include <gflags/gflags.h>

#include <orion/bosen/driver.hpp>
#include <orion/bosen/byte_buffer.hpp>
#include <orion/bosen/type.hpp>

DEFINE_string(master_ip, "127.0.0.1",
              "IP address that the master thread listens to for "
              "incoming connections");
DEFINE_int32(master_port, 10000,
             "port that the master thread listens to for "
             "incoming connections");
DEFINE_uint64(comm_buff_capacity, 1024 * 4, "communication buffer capacity");

DEFINE_string(lib_path, "", "orion library path");

int
main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  LOG(INFO) << "hello, driver is started!";

  orion::bosen::DriverConfig driver_config(
      FLAGS_master_ip.c_str(), FLAGS_master_port,
      FLAGS_comm_buff_capacity);

  orion::bosen::Driver driver(driver_config);
  driver.ConnectToMaster();
  driver.Stop();
}
