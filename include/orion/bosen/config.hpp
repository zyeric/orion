#pragma once

#include <string>
#include <glog/logging.h>

namespace orion {
namespace bosen {

struct Config {
 public:
  const size_t kNumExecutors;
  const size_t kNumServers;
  const size_t kNumExecutorsPerWorker;
  const size_t kNumServersPerWorker;
  const std::string kMasterIp;
  const uint16_t kMasterPort;
  const std::string kWorkerIp;
  const uint16_t kWorkerPort;
  const size_t kCommBuffCapacity {1024 * 4};
  const int32_t kWorkerId;
  const size_t kExecutorThreadPoolSize;
  const size_t kPartitionSizeMB;
  const std::string kHdfsNameNode;
  const std::string kOrionHome;

  Config(size_t num_executors,
         size_t num_servers,
         size_t num_executors_per_worker,
         size_t num_servers_per_worker,
         const std::string master_ip,
         uint16_t master_port,
         const std::string &worker_ip,
         uint16_t worker_port,
         uint64_t comm_buff_capacity,
         int32_t worker_id,
         size_t executor_thread_pool_size,
         size_t partition_size_mb,
         const std::string &hdfs_name_node,
         const std::string &orion_home):
      kNumExecutors(num_executors),
      kNumServers(num_servers),
      kNumExecutorsPerWorker(num_executors_per_worker),
      kNumServersPerWorker(num_servers_per_worker),
      kMasterIp(master_ip),
      kMasterPort(master_port),
      kWorkerIp(worker_ip),
      kWorkerPort(worker_port),
      kCommBuffCapacity(comm_buff_capacity),
      kWorkerId(worker_id),
      kExecutorThreadPoolSize(executor_thread_pool_size),
      kPartitionSizeMB(partition_size_mb),
      kHdfsNameNode(hdfs_name_node),
      kOrionHome(orion_home) { }
};

}
}
