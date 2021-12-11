import robotoc
import numpy as np
import math


LF_foot_id = 12
LH_foot_id = 22
RF_foot_id = 32
RH_foot_id = 42
contact_frames = [LF_foot_id, LH_foot_id, RF_foot_id, RH_foot_id] 
path_to_urdf = '../anymal_b_simple_description/urdf/anymal.urdf'
baumgarte_time_step = 0.04
robot = robotoc.Robot(path_to_urdf, robotoc.BaseJointType.FloatingBase, 
                      contact_frames, baumgarte_time_step)

dt = 0.02
step_length = 0.15
step_height = 0.1
swing_time = 0.5
double_support_time = 0.04
t0 = double_support_time 
cycle = 3

# Create the cost function
cost = robotoc.CostFunction()
q_standing = np.array([0, 0, 0.4792, 0, 0, 0, 1, 
                       -0.1,  0.7, -1.0, 
                       -0.1, -0.7,  1.0, 
                        0.1,  0.7, -1.0, 
                        0.1, -0.7,  1.0])
q_weight = np.array([0, 0, 0, 250000, 250000, 250000, 
                     0.0001, 0.0001, 0.0001, 
                     0.0001, 0.0001, 0.0001,
                     0.0001, 0.0001, 0.0001,
                     0.0001, 0.0001, 0.0001])
v_weight = np.array([100, 100, 100, 100, 100, 100, 
                     1, 1, 1, 
                     1, 1, 1,
                     1, 1, 1,
                     1, 1, 1])
u_weight = np.full(robot.dimu(), 1.0e-01)
qi_weight = np.array([1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 
                      100, 100, 100, 
                      100, 100, 100,
                      100, 100, 100,
                      100, 100, 100])
vi_weight = np.full(robot.dimv(), 100)
config_cost = robotoc.ConfigurationSpaceCost(robot)
config_cost.set_q_ref(q_standing)
config_cost.set_q_weight(q_weight)
config_cost.set_qf_weight(q_weight)
config_cost.set_qi_weight(qi_weight)
config_cost.set_v_weight(v_weight)
config_cost.set_vf_weight(v_weight)
config_cost.set_vi_weight(vi_weight)
config_cost.set_u_weight(u_weight)
cost.push_back(config_cost)

robot.forward_kinematics(q_standing)
x3d0_LF = robot.frame_position(LF_foot_id)
x3d0_LH = robot.frame_position(LH_foot_id)
x3d0_RF = robot.frame_position(RF_foot_id)
x3d0_RH = robot.frame_position(RH_foot_id)
LF_t0 = t0 + swing_time + double_support_time
LH_t0 = t0
RF_t0 = t0
RH_t0 = t0 + swing_time + double_support_time
LF_foot_ref = robotoc.PeriodicFootTrackRef(x3d0_LF, step_length, step_height, 
                                           LF_t0, swing_time, 
                                           swing_time+2*double_support_time, False)
LH_foot_ref = robotoc.PeriodicFootTrackRef(x3d0_LH, step_length, step_height, 
                                           LH_t0, swing_time, 
                                           swing_time+2*double_support_time, True)
RF_foot_ref = robotoc.PeriodicFootTrackRef(x3d0_RF, step_length, step_height, 
                                           RF_t0, swing_time, 
                                           swing_time+2*double_support_time, True)
RH_foot_ref = robotoc.PeriodicFootTrackRef(x3d0_RH, step_length, step_height, 
                                           RH_t0, swing_time, 
                                           swing_time+2*double_support_time, False)
LF_cost = robotoc.TimeVaryingTaskSpace3DCost(robot, LF_foot_id, LF_foot_ref)
LH_cost = robotoc.TimeVaryingTaskSpace3DCost(robot, LH_foot_id, LH_foot_ref)
RF_cost = robotoc.TimeVaryingTaskSpace3DCost(robot, RF_foot_id, RF_foot_ref)
RH_cost = robotoc.TimeVaryingTaskSpace3DCost(robot, RH_foot_id, RH_foot_ref)
foot_track_weight = np.full(3, 1.0e06)
LF_cost.set_x3d_weight(foot_track_weight)
LH_cost.set_x3d_weight(foot_track_weight)
RF_cost.set_x3d_weight(foot_track_weight)
RH_cost.set_x3d_weight(foot_track_weight)
cost.push_back(LF_cost)
cost.push_back(LH_cost)
cost.push_back(RF_cost)
cost.push_back(RH_cost)

com_ref0 = (x3d0_LF + x3d0_LH + x3d0_RF + x3d0_RH) / 4
com_ref0[2] = robot.com()[2]
vcom_ref = np.zeros(3)
vcom_ref[0] = 0.5 * step_length / swing_time
com_ref = robotoc.PeriodicCoMRef(com_ref0, vcom_ref, t0, swing_time, 
                                 double_support_time, True)
com_cost = robotoc.TimeVaryingCoMCost(robot, com_ref)
com_cost.set_com_weight(np.full(3, 1.0e06))
cost.push_back(com_cost)

# Create the constraints
constraints           = robotoc.Constraints(barrier=1.0e-03, fraction_to_boundary_rule=0.995)
joint_position_lower  = robotoc.JointPositionLowerLimit(robot)
joint_position_upper  = robotoc.JointPositionUpperLimit(robot)
joint_velocity_lower  = robotoc.JointVelocityLowerLimit(robot)
joint_velocity_upper  = robotoc.JointVelocityUpperLimit(robot)
joint_torques_lower   = robotoc.JointTorquesLowerLimit(robot)
joint_torques_upper   = robotoc.JointTorquesUpperLimit(robot)
mu = 0.7
friction_cone         = robotoc.FrictionCone(robot, mu)
constraints.push_back(joint_position_lower)
constraints.push_back(joint_position_upper)
constraints.push_back(joint_velocity_lower)
constraints.push_back(joint_velocity_upper)
constraints.push_back(joint_torques_lower)
constraints.push_back(joint_torques_upper)
constraints.push_back(friction_cone)

# Create the contact sequence
max_num_impulses = 2*cycle
contact_sequence = robotoc.ContactSequence(robot, max_num_impulses)

contact_points = [x3d0_LF, x3d0_LH, x3d0_RF, x3d0_RH]
contact_status_standing = robot.create_contact_status()
contact_status_standing.activate_contacts([0, 1, 2, 3])
contact_status_standing.set_contact_points(contact_points)
contact_sequence.init_contact_sequence(contact_status_standing)

contact_status_lhrf_swing = robot.create_contact_status()
contact_status_lhrf_swing.activate_contacts([0, 3])
contact_status_lhrf_swing.set_contact_points(contact_points)
contact_sequence.push_back(contact_status_lhrf_swing, t0)

contact_points[1][0] += 0.5 * step_length
contact_points[2][0] += 0.5 * step_length
contact_status_standing.set_contact_points(contact_points)
contact_sequence.push_back(contact_status_standing, t0+swing_time)

contact_status_lfrh_swing = robot.create_contact_status()
contact_status_lfrh_swing.activate_contacts([1, 2])
contact_status_lfrh_swing.set_contact_points(contact_points)
contact_sequence.push_back(contact_status_lfrh_swing, 
                           t0+swing_time+double_support_time)

contact_points[0][0] += step_length
contact_points[3][0] += step_length
contact_status_standing.set_contact_points(contact_points)
contact_sequence.push_back(contact_status_standing, 
                           t0+2*swing_time+double_support_time)

for i in range(cycle-1):
    t1 = t0 + (i+1)*(2*swing_time+2*double_support_time)
    contact_status_lhrf_swing.set_contact_points(contact_points)
    contact_sequence.push_back(contact_status_lhrf_swing, t1)

    contact_points[1][0] += step_length
    contact_points[2][0] += step_length
    contact_status_standing.set_contact_points(contact_points)
    contact_sequence.push_back(contact_status_standing, t1+swing_time)

    contact_status_lfrh_swing.set_contact_points(contact_points)
    contact_sequence.push_back(contact_status_lfrh_swing, 
                               t1+swing_time+double_support_time)

    contact_points[0][0] += step_length
    contact_points[3][0] += step_length
    contact_status_standing.set_contact_points(contact_points)
    contact_sequence.push_back(contact_status_standing, 
                               t1+2*swing_time+double_support_time)

# you can chech the contact sequence as 
# print(contact_sequence)

T = t0 + cycle*(2*double_support_time+2*swing_time)
N = math.floor(T/dt) 
ocp = robotoc.OCP(robot=robot, cost=cost, constraints=constraints, 
                  T=T, N=N, max_num_each_discrete_events=max_num_impulses)
solver_options = robotoc.SolverOptions()
ocp_solver = robotoc.OCPSolver(ocp=ocp, contact_sequence=contact_sequence, 
                               solver_options=solver_options, nthreads=4)

# Initial time and intial state 
t = 0.
q = q_standing
v = np.zeros(robot.dimv())

ocp_solver.set_solution("q", q)
ocp_solver.set_solution("v", v)
f_init = np.array([0.0, 0.0, 0.25*robot.total_weight()])
ocp_solver.set_solution("f", f_init)

ocp_solver.init_constraints(t)
print("Initial KKT error: ", ocp_solver.KKT_error(t, q, v))
ocp_solver.solve(t, q, v)
print("KKT error after convergence: ", ocp_solver.KKT_error(t, q, v))
print(ocp_solver.get_solver_statistics())

# num_iteration = 1000
# robotoc.utils.benchmark.cpu_time(ocp_solver, t, q, v, num_iteration)

viewer = robotoc.utils.TrajectoryViewer(path_to_urdf=path_to_urdf, 
                                        base_joint_type=robotoc.BaseJointType.FloatingBase,
                                        viewer_type='gepetto')
viewer.set_contact_info(contact_frames, mu)
discretization = ocp_solver.get_time_discretization()
print(discretization)
viewer.display(discretization.time_steps(), ocp_solver.get_solution('q'), 
               ocp_solver.get_solution('f', 'WORLD'))