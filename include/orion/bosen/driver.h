#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stdint.h>
#include <stddef.h>
#include <julia.h>
#include "constants.h"

extern "C" {
  void orion_init(
      const char *master_ip,
      uint16_t master_port,
      size_t comm_buff_capacity);

  void orion_connect_to_master();

  jl_value_t* orion_eval_expr_on_all(
      const uint8_t* expr,
      size_t expr_size,
      int32_t module);

  void orion_create_dist_array(
      int32_t id,
      int32_t parent_type,
      int32_t map_type,
      bool flatten_results,
      size_t num_dims,
      int32_t value_type,
      const char* file_path,
      int32_t parent_id,
      int32_t init_type,
      int32_t mapper_func_module,
      const char* mapper_func_name,
      int64_t* dims,
      int32_t random_init_type,
      bool is_dense,
      const char* symbol);

  void orion_repartition_dist_array(
      int32_t id,
      const char *partition_func_name,
      int32_t partition_scheme,
      int32_t index_type);

  void orion_exec_for_loop(
      int32_t iteration_space_id,
      int32_t parallel_scheme,
      const int32_t *space_partitioned_dist_array_ids,
      size_t num_space_partitioned_dist_arrays,
      const int32_t *time_partitioned_dist_array_ids,
      size_t num_time_partitioned_dist_arrays,
      const int32_t *global_indexed_dist_array_ids,
      size_t num_global_indexed_dist_arrays,
      const char *loop_batch_func_name,
      bool is_ordered);

  jl_value_t* orion_get_accumulator_value(
      const char *symbol,
      const char *combiner);

  void orion_stop();

  bool orion_glogconfig_set(const char* key, const char* value);
  void orion_glogconfig_set_progname(const char* progname);
  void orion_glog_init();
}

#endif
