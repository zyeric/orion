include("/home/ubuntu/orion/src/julia/orion.jl")

const K = 100
const num_iterations = 10
step_size = 0.001

# set path to the C++ runtime library
Orion.set_lib_path("/home/ubuntu/orion/lib/liborion_driver.so")
Orion.load_constants()
# test library path
Orion.helloworld()

ratings = Orion.DistArray{Float32}()
W = Orion.DistArray{Float32}()
H = Orion.DistArray{Float32}()
ratings.num_dims = 2
W.id = 1
H.id = 2
W.num_dims = 2
H.num_dims = 2
ratings.dims = [6000, 4000]
W.dims = [6000, 100]
H.dims = [4000, 100]

for i = 1:num_iterations
    Orion.@parallel_for for rating in rating
s	x_idx = rating[1][1]
	y_idx = rating[1][2]
	rv = rating[2]

        W_row = W[x_idx, :]
	H_row = H[y_idx, :]
	pred = dot(W_row, H_row)
	diff = rv - pred
	W_grad = -2 * diff .* H_row
	H_grad = -2 * diff .* W_row
	W[x_idx, :] = W_row - step_size .* W_grad
	H[y_idx, :] = H_row - step_size .*H_grad .* K
    end
    step_size = step_size * 0.9
end

#ratings = Orion.text_file(data_path, parse_line)
