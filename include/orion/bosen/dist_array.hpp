#pragma once

#include <map>
#include <unordered_map>
#include <orion/noncopyable.hpp>
#include <orion/bosen/type.hpp>
#include <orion/bosen/config.hpp>
#include <orion/bosen/julia_module.hpp>
#include <orion/bosen/blob.hpp>
#include <orion/bosen/dist_array_meta.hpp>
#include <orion/bosen/send_data_buffer.hpp>
#include <orion/bosen/peer_recv_buffer.hpp>
#include <orion/bosen/julia_thread_requester.hpp>
#include <orion/bosen/perf.hpp>

namespace orion {
namespace bosen {
class AbstractDistArrayPartition;
class JuliaEvaluator;

// A DistArray has different access modes depending on
// 1) whether or not a global or local index is built;
// 1) whether it's dense or sparse

// The supported access modes are:
// 1) Seqential: if not index is built
// 2) LocalDenseIndex: if local index is built and dist array partition is dense
// 3) LocalSparseIndex: if local index is built and dist array partition is sparse
// 4) GlobalIndex: if global index is built

// Sequential:
// 1) int64* GetKeys(Partition)
// 2) ValueType* GetValues(Partition)

// LocalDenseIndex:
// 1) ValueType* GetRange()

// LocalSparseIndex:

// GlobalIndex

class DistArray {
 public:
  using TimePartitionMap = std::map<int32_t, AbstractDistArrayPartition*>;
  using SpaceTimePartitionMap = std::map<int32_t, TimePartitionMap>;
  using PartitionMap = std::map<int32_t, AbstractDistArrayPartition*>;
  // using raw pointers to transfer ownership to sender threads which will free
  // the memory
 public:
  const int32_t kId;
  const Config &kConfig;
  const bool kIsServer;
  const type::PrimitiveType kValueType;
  const size_t kValueSize;
  const int32_t kExecutorId;
  const int32_t kServerId;
 private:
  SpaceTimePartitionMap space_time_partitions_;
  PartitionMap partitions_;
  DistArrayMeta meta_;
  JuliaThreadRequester *julia_requester_;
  std::vector<jl_value_t*> gc_partitions_;
  std::unique_ptr<AbstractDistArrayPartition> buffer_partition_;
  jl_value_t* dist_array_jl_ {nullptr};
  std::vector<int64_t> dim_key_buff_;

  const std::vector<PerfCount::CountType> kPerfCountTypes {
    PerfCount::PERF_COUNT_TYPE_HW_CPU_CYCLES,
        PerfCount::PERF_COUNT_TYPE_HW_INSTRUCTIONS,
        PerfCount::PERF_COUNT_TYPE_HW_CACHE_REFERENCES,
        PerfCount::PERF_COUNT_TYPE_HW_CACHE_MISSES,
        PerfCount::PERF_COUNT_TYPE_HW_CACHE_L1D_READ_ACCESS,
        PerfCount::PERF_COUNT_TYPE_HW_CACHE_L1D_WRITE_ACCESS };
  PerfCount perf_count_;

 public:
  DistArray(int32_t id,
            const Config& config,
            bool is_server,
            type::PrimitiveType value_type,
            int32_t executor_id,
            int32_t server_id,
            size_t num_dims,
            DistArrayParentType parent_type,
            DistArrayInitType init_type,
            DistArrayMapType map_type,
            DistArrayPartitionScheme partition_scheme,
            JuliaModule map_func_module,
            const std::string &map_func_name,
            type::PrimitiveType random_init_type,
            bool flatten_results,
            bool is_dense,
            const std::string &symbol,
            JuliaThreadRequester *julia_requester,
            const std::string &key_func_name);
  ~DistArray();
  int32_t GetId() const { return kId; }
  jl_value_t* GetJuliaDistArray() { return dist_array_jl_; }
  void SetJuliaDistArray(jl_value_t* dist_array_jl) { dist_array_jl_ = dist_array_jl; }

  void LoadPartitionsFromTextFile(std::string file_path);
  void ParseBufferedText(Blob *max_ids,
                         const std::vector<size_t> &line_number_start);
  void GetPartitionTextBufferNumLines(std::vector<int64_t> *partition_ids,
                                      std::vector<size_t> *num_lines);
  void Init();
  void Map(DistArray *child_dist_array);
  void GroupBy(DistArray *child_dist_array);

  void ComputeModuloRepartition(size_t num_partitions);
  void ComputeRepartition(const std::string &repartition_func_name);
  void ComputeHashRepartition(const std::string &repartition_func_name,
                              size_t num_partitions);
  void ComputePartialModuloRepartitionIdsAndRepartition(size_t num_partitions,
                                                        const std::vector<size_t> &dim_indices);
  void ComputePartialRandomRepartitionIdsAndRepartition(size_t num_partitions,
                                                        const std::vector<size_t> &dim_indices);

  void GetPartitionNumUniquePartialKeys(const std::vector<size_t> &dim_indices,
                                        std::vector<int32_t> *partition_ids,
                                        std::vector<size_t> *partition_num_unique_keys) const;

  void RandomRemapPartialKeys(const std::vector<size_t> &dim_indices,
                              const std::vector<int32_t> &partition_ids,
                              const std::vector<int64_t> &remapped_ranges);

  void ComputeHistogram(size_t dim_index,
                        size_t num_bins,
                        std::unordered_map<int64_t, size_t> *histogram) const;

  void SaveAsTextFile(const std::string &to_string_func_name,
                      const std::string &file_path);

  void SetDims(const std::vector<int64_t> &dims);
  void SetDims(const int64_t* dims, size_t num_dims);
  const std::vector<int64_t> &GetDims() const;
  DistArrayMeta &GetMeta();
  type::PrimitiveType GetValueType();
  AbstractDistArrayPartition *GetLocalPartition(int32_t partition_id);
  AbstractDistArrayPartition *GetLocalPartition(int32_t space_id,
                                                int32_t time_id);
  std::pair<AbstractDistArrayPartition*, bool> GetAndCreateLocalPartition(int32_t partition_id);
  std::pair<AbstractDistArrayPartition*, bool> GetAndCreateLocalPartition(int32_t space_id,
                                                                          int32_t time_id);
  void GetAndClearLocalPartitions(std::vector<AbstractDistArrayPartition*>
                                  *buff);

  AbstractDistArrayPartition *CreatePartition();
  void RepartitionSerializeAndClear(ExecutorSendBufferMap* send_buff_ptr);
  void RepartitionDeserialize(PeerRecvRepartitionDistArrayDataBuffer *data_buff_ptr);

  void CheckAndBuildIndex();

  void GetAndSerializeValue(int64_t key, Blob *bytes_buff);
  void GetAndSerializeValues(int64_t *keys,
                             size_t num_keys,
                             Blob *bytes_buff);

  void GetMaxPartitionIds(std::vector<int32_t>* ids);
  void DeletePartition(int32_t partition_id);

  void CreateDistArrayBuffer(const std::string &serialized_value_type);
  AbstractDistArrayPartition* GetBufferPartition();

  std::vector<int64_t> &GetDimKeyBuffer();
 private:
  DISALLOW_COPY(DistArray);
  void RepartitionSerializeAndClearSpaceTime(ExecutorSendBufferMap* send_buff_ptr);
  void RepartitionSerializeAndClear1D(ExecutorSendBufferMap* send_buff_ptr);

  void RepartitionDeserializeInternal(uint8_t *mem, size_t mem_size);
  void RepartitionDeserializeSpaceTime(uint8_t *mem, size_t mem_size);
  void RepartitionDeserialize1D(uint8_t *mem, size_t mem_size);
  void GetMaxPartitionIdsSpaceTime(std::vector<int32_t>* ids);
  void GetMaxPartitionIds1D(std::vector<int32_t>* ids);
  void BuildPartitionIndices();
  void BuildPartitionKeyValueBuffersFromSparseIndex();
  void ComputeMaxPartitionIds();
  void ComputeMaxPartitionIdsSpaceTime();
  void ComputeMaxPartitionIds1D();

  void ComputeHistogramSpaceTime(size_t dim_index,
                                 size_t num_bins,
                                 std::unordered_map<int64_t, size_t> *histogram,
                                 size_t full_bin_size,
                                 size_t num_full_bins,
                                 size_t bin_size,
                                 int64_t full_bin_cutoff_key,
                                 int64_t dim_divider,
                                 int64_t dim_mod) const;

  void ComputeHistogram1D(size_t dim_index,
                          size_t num_bins,
                          std::unordered_map<int64_t, size_t> *histogram,
                          size_t full_bin_size,
                          size_t num_full_bins,
                          size_t bin_size,
                          int64_t full_bin_cutoff_key,
                          int64_t dim_divider,
                          int64_t dim_mod) const;

  void PrintPerfCounts();
};

}
}
