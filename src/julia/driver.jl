export helloworld, local_helloworld, glog_init

function helloworld()
    ccall((:orion_helloworld, lib_path), Void, ())
end

function local_helloworld()
    println("hello world!")
end

function glog_init()
    ccall((:orion_glog_init, lib_path), Void, ())
end

function init(
    master_ip::AbstractString,
    master_port::Integer,
    comm_buff_capacity::Integer,
    _num_executors::Integer,
    _num_servers::Integer)
    ccall((:orion_init, lib_path),
          Void, (Cstring, UInt16, UInt64), master_ip, master_port,
          comm_buff_capacity)
    global const num_executors = _num_executors
    global const num_servers = _num_servers
    load_constants()
end

function execute_code(
    executor_id::Integer,
    code::AbstractString,
    ResultType::DataType)
    result_buff = Array{ResultType}(1)
    ccall((:orion_execute_code_on_one, lib_path),
          Void, (Int32, Cstring, Int32, Ptr{Void}),
          executor_id, code, data_type_to_int32(ResultType),
          result_buff);
    return result_buff[1]
end

function stop()
    ccall((:orion_stop, lib_path), Void, ())
end

function eval_expr_on_all(ex, eval_module::Symbol)
    buff = IOBuffer()
    serialize(buff, ex)
    buff_array = take!(buff)
    println("serialized expr length = ", length(buff_array))
    result_array = ccall((:orion_eval_expr_on_all, lib_path),
                         Any, (Ref{UInt8}, UInt64, Int32),
                         buff_array, length(buff_array),
                         module_to_int32(eval_module))
    #result_array = Base.cconvert(Array{Array{UInt8}}, result_bytes)
    return result_array
end

function define_var(var::Symbol)
    @assert isdefined(current_module(), var)
    value = eval(current_module(), var)

    expr = :($var = $value)

    eval_expr_on_all(expr, :Main)
end

function define_vars(var_set::Set{Symbol})
    for var in var_set
        define_var(var)
    end
end

function exec_for_loop(exec_for_loop_id::Int32,
                       iteration_space_id::Integer,
                       parallel_scheme::ForLoopParallelScheme,
                       space_partitioned_dist_array_ids::Vector{Int32},
                       time_partitioned_dist_array_ids::Vector{Int32},
                       global_indexed_dist_array_ids::Vector{Int32},
                       dist_array_buffer_ids::Vector{Int32},
                       written_dist_array_ids::Vector{Int32}, # dist arrays that are directly written up (not through buffer)
                       accessed_dist_array_ids::Vector{Int32}, # dist arrays that are directly accessed
                       global_read_only_vars::Vector{Symbol},
                       accumulator_vars::Vector{Symbol},
                       loop_batch_func_name::AbstractString,
                       prefetch_batch_func_name::AbstractString,
                       is_ordered::Bool,
                       is_repeated::Bool)
    println("exec_for_loop for ", iteration_space_id, " ", parallel_scheme)
    global_read_only_var_vals = Vector{Vector{UInt8}}(length(global_read_only_vars))
    index = 1
    for var_sym in global_read_only_vars
        var_val = eval(current_module(), var_sym)
        buff = IOBuffer()
        serialize(buff, var_val)
        buff_array = take!(buff)
        global_read_only_var_vals[index] = buff_array
        index += 1
    end

    accumulator_var_syms = Vector{String}(length(accumulator_vars))
    index = 1
    for var_sym in accumulator_vars
        var_sym_str = string(var_sym)
        accumulator_var_syms[index] = var_sym_str
        index += 1
    end

    ccall((:orion_exec_for_loop, lib_path),
          Void, (Int32,
                 Int32,
                 Int32,
                 Ref{Int32}, UInt64,
                 Ref{Int32}, UInt64,
                 Ref{Int32}, UInt64,
                 Ref{Int32}, UInt64,
                 Ref{Int32}, UInt64,
                 Ref{Int32}, UInt64,
                 Any,
                 Ref{Cstring}, UInt64,
                 Cstring, Cstring, Bool, Bool),
          exec_for_loop_id,
          iteration_space_id,
          for_loop_parallel_scheme_to_int32(parallel_scheme),
          space_partitioned_dist_array_ids, length(space_partitioned_dist_array_ids),
          time_partitioned_dist_array_ids, length(time_partitioned_dist_array_ids),
          global_indexed_dist_array_ids, length(global_indexed_dist_array_ids),
          dist_array_buffer_ids, length(dist_array_buffer_ids),
          written_dist_array_ids, length(written_dist_array_ids),
          accessed_dist_array_ids, length(accessed_dist_array_ids),
          global_read_only_var_vals,
          accumulator_var_syms, length(accumulator_var_syms),
          loop_batch_func_name, prefetch_batch_func_name, is_ordered, is_repeated)
end

function get_aggregated_value(var_sym::Symbol, combiner_func::Symbol)
    @assert which(combiner_func) == Base
    combiner_str = string(combiner_func)
    value = ccall((:orion_get_accumulator_value, lib_path),
                  Any, (Cstring, Cstring),
                  string(var_sym), combiner_str)

    return value
end

function reset_var_value(var_sym::Symbol, value)
    expr = :($var_sym = $value)
    eval_expr_on_all(expr, :Main)
end

function reset_accumulator(var_sym::Symbol)
    reset_var_value(var_sym, accumulator_info_dict[var_sym].init_value)
end

type DistArrayBufferInfo
    dist_array_id::Int32
    apply_buffer_func_name::Symbol
    helper_dist_array_buffer_ids::Vector{Int32}
    helper_dist_array_ids::Vector{Int32}
    delay_mode::DistArrayBufferDelayMode
    max_delay::UInt64
    DistArrayBufferInfo(dist_array_id::Int32,
                        apply_buffer_func_name::Symbol,
                        delay_mode::DistArrayBufferDelayMode,
                        max_delay::UInt64) =
                            new(dist_array_id,
                                apply_buffer_func_name,
                                Vector{Int32}(),
                                Vector{Int32}(),
                                delay_mode,
                                max_delay)
end

dist_array_buffer_map = Dict{Int32, DistArrayBufferInfo}()

function set_write_buffer(dist_array_buffer::DistArrayBuffer,
                          dist_array::DistArray,
                          apply_func::Function,
                          helpers...;
                          max_delay::Integer = UInt64(0),
                          auto_delay::Bool = false)
    apply_func_name = Base.function_name(apply_func)
    apply_updates_batch_func_name = gen_unique_symbol()

    delay_mode::DistArrayBufferDelayMode = DistArrayBufferDelayMode_default
    if auto_delay
        delay_mode = DistArrayBufferDelayMode_auto
    elseif max_delay > 0
        delay_mode = DistArrayBufferDelayMode_max_delay
    end
    buffer_info = DistArrayBufferInfo(dist_array.id,
                                      apply_updates_batch_func_name,
                                      delay_mode,
                                      UInt64(max_delay))
    println(buffer_info)
    for helper in helpers
        if isa(helper, DistArrayBuffer)
            push!(buffer_info.helper_dist_array_buffer_ids, helper.id)
        else
            push!(buffer_info.helper_dist_array_ids, helper.id)
        end
    end
    dist_array_buffer_map[dist_array_buffer.id] = buffer_info
    ccall((:orion_set_dist_array_buffer_info, lib_path),
          Void, (Int32, Int32, Cstring,
                 Ptr{Int32}, UInt64,
                 Ptr{Int32}, UInt64,
                 Int32, UInt64),
          dist_array_buffer.id,
          buffer_info.dist_array_id,
          string(buffer_info.apply_buffer_func_name),
          buffer_info.helper_dist_array_buffer_ids,
          length(buffer_info.helper_dist_array_buffer_ids),
          buffer_info.helper_dist_array_ids,
          length(buffer_info.helper_dist_array_ids),
          dist_array_buffer_delay_mode_to_int32(buffer_info.delay_mode),
          buffer_info.max_delay)

    num_helper_dist_arrays = length(buffer_info.helper_dist_array_ids)
    num_helper_dist_array_buffers = length(buffer_info.helper_dist_array_buffer_ids)

    apply_updates_batch_func = gen_apply_batch_buffered_updates_func(
        apply_updates_batch_func_name,
        apply_func_name,
        num_helper_dist_arrays,
        num_helper_dist_array_buffers)
    eval_expr_on_all(apply_updates_batch_func, :Main)
end

function reset_write_buffer(dist_array_buffer::DistArrayBuffer)
    delete!(dist_array_buffer_map, dist_array_buffer.id)
    ccall((:orion_delete_dist_array_buffer_info, lib_path),
          Void, (Int32,),
          dist_array_buffer.id)
end
