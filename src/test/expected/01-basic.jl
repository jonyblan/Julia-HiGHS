using JuMP
using HiGHS
using LinearAlgebra
model = Model(HiGHS.Optimizer)
@variable(model, a <= 5)
@objective(model, Max, a)
optimize!(model)
if has_values(model)
    println(value(a))
else
    println("No primal solution; status = ", termination_status(model))
end
