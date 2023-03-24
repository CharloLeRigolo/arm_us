#include "ArmUs.hpp"

ArmUs::ArmUs(ControlMode controlMode) : m_controlMode(controlMode)
{
    if (controlMode == ControlMode::Real)
    {
        m_arm_us_info = std::make_unique<ArmUsInfoReal>(std::bind(&ArmUs::call_inv_kin_calc_server, this));
    }
    else if (controlMode == ControlMode::Simulation)
    {
        m_arm_us_info = std::make_unique<ArmUsInfoSimul>(std::bind(&ArmUs::call_inv_kin_calc_server, this));
    }
}

void ArmUs::Run()
{
    ros::Rate loop_rate(ROS_RATE);

    while (ros::ok())
    {
        m_arm_us_info->calculate_motor_velocities();
        m_arm_us_info->calculate_joint_angles();

        if (!ros::ok())
        {
            send_cmd_motor_stop();
        }
        else 
        {
            send_cmd_motor();
        }

        send_gui_info();
        send_3d_graph_info();

        ros::spinOnce();
        loop_rate.sleep();
    }

    send_cmd_motor_stop();
    ros::shutdown();
}

void ArmUs::Initalize()
{
    m_sub_input =         m_nh.subscribe("joy", 1, &ArmUs::subControllerCallback, this);
    m_sub_gui =           m_nh.subscribe("gui_arm_us_chatter", 1, &ArmUs::sub_gui_callback, this);
    if (m_controlMode == ControlMode::Real)
    {
        m_sub_joint_states =  m_nh.subscribe("joint_states", 1, &ArmUs::sub_joint_states_callback, this);
    }
    
    m_pub_motor =         m_nh.advertise<sensor_msgs::JointState>("desired_joint_states", 10);
    m_pub_gui =           m_nh.advertise<arm_us::GuiInfo>("gui_info", 10);
    m_pub_3d_graph =      m_nh.advertise<arm_us::GraphInfo>("graph_info", 10);

    m_client_inv_kin_calc = m_nh.serviceClient<arm_us::InverseKinematicCalc>("inverse_kinematic_calc_server");

    setParams();
}

void ArmUs::subControllerCallback(const sensor_msgs::Joy::ConstPtr &data)
{
    
    m_controller.JoyLeft.set(data->axes[LEFT_JOY_VERT], data->axes[LEFT_JOY_HORI]);
    m_controller.JoyRight.set(data->axes[RIGHT_JOY_VERT], data->axes[RIGHT_JOY_HORI]);

    /*
    m_controller.Pad.set(data->axes[PAD_VERT], data->axes[PAD_HORI]);
    m_controller.Buttons.set(data->buttons[BUTTON_1], data->buttons[BUTTON_2], data->buttons[BUTTON_3], data->buttons[BUTTON_4]);
    m_controller.Bumpers.set(data->buttons[LEFT_BUMP], data->buttons[RIGHT_BUMP]);
    m_controller.Triggers.set(data->buttons[LEFT_TRIG], data->buttons[RIGHT_TRIG]);
    m_controller.DisplayControllerInputs();
    */

    // Switch modes (cartesian and joint) with controller
    if (data->buttons[BUTTON_3] == 1 && m_controller.Buttons.Button3 == 0)
    {
        if (m_arm_us_info->MoveMode == MovementMode::Cartesian)
        {
            m_arm_us_info->MoveMode = MovementMode::Joint;
            ROS_WARN("Joint");
        }
        else 
        {
            m_arm_us_info->MoveMode = MovementMode::Cartesian;
            ROS_WARN("Cartesian");
        }
    }
    
    /********** Joint **********/
    if (m_arm_us_info->MoveMode == MovementMode::Joint)
    {
        // Change joint controlled with controller
        if (data->buttons[BUTTON_4] == 1 && m_controller.Buttons.Button4 == 0)
        {
            m_arm_us_info->JointControlled++;
            if (m_arm_us_info->JointControlled > 5)
            {
                m_arm_us_info->JointControlled = 1;
            }
            ROS_WARN("Joint controlled : %d", m_arm_us_info->JointControlled);
        }

        if (data->buttons[BUTTON_2] == 1 && m_controller.Buttons.Button2 == 0)
        {
            m_arm_us_info->JointControlled--;
            if (m_arm_us_info->JointControlled < 1)
            {
                m_arm_us_info->JointControlled = 5;
            }
            ROS_WARN("Joint controlled : %d", m_arm_us_info->JointControlled);
        }

        // Set speed of joint
        m_arm_us_info->JointCommand = m_controller.JoyLeft.Vertical * MAX_VEL;

    }
    /********** Cartesian **********/
    else if (m_arm_us_info->MoveMode == MovementMode::Cartesian)
    {
        m_arm_us_info->CartesianCommand.x += m_controller.JoyLeft.Vertical * MAX_VEL;
        m_arm_us_info->CartesianCommand.y += m_controller.JoyLeft.Horizontal * MAX_VEL;
        m_arm_us_info->CartesianCommand.z += m_controller.JoyRight.Vertical * MAX_VEL;

        // ROS_WARN("x = %f, y = %f, z = %f", m_arm_us_info->CartesianCommand.x, m_arm_us_info->CartesianCommand.y, m_arm_us_info->CartesianCommand.z);
    }

    m_controller.Buttons.set(data->buttons[BUTTON_1], data->buttons[BUTTON_2], data->buttons[BUTTON_3], data->buttons[BUTTON_4]);
}

void ArmUs::sub_gui_callback(const arm_us::GuiFeedback::ConstPtr &data)
{
    if (data->joint)
    {
        m_arm_us_info->MoveMode = MovementMode::Joint;
    }
    else if (data->cartesian)
    {
        m_arm_us_info->MoveMode = MovementMode::Cartesian;
    }

    m_arm_us_info->JointControlled = data->joint_controlled;
}

void ArmUs::sub_joint_states_callback(const sensor_msgs::JointState::ConstPtr &data)
{
    m_arm_us_info->MotorPositions.set(data->position[0], data->position[1], data->position[2], data->position[3], data->position[4]);
    m_arm_us_info->MotorVelocities.set(data->velocity[0], data->velocity[1], data->velocity[2], data->velocity[3], data->velocity[4]);

    m_arm_us_info->PositionDifference = data->position[0] - data->position[1];
}

void ArmUs::setParams()
{   
    m_nh.getParam("/master_node/left_joy_hori", LEFT_JOY_HORI);
    m_nh.getParam("/master_node/left_joy_vert", LEFT_JOY_VERT);
    
    m_nh.getParam("/master_node/right_joy_hori", RIGHT_JOY_HORI);
    m_nh.getParam("/master_node/right_joy_vert", RIGHT_JOY_VERT);

    m_nh.getParam("/master_node/pad_hori", PAD_HORI);
    m_nh.getParam("/master_node/pad_vert", PAD_VERT);


    m_nh.getParam("/master_node/button_1", BUTTON_1);
    m_nh.getParam("/master_node/button_2", BUTTON_2);
    m_nh.getParam("/master_node/button_3", BUTTON_3);
    m_nh.getParam("/master_node/button_4", BUTTON_4);

    m_nh.getParam("/master_node/left_bump", LEFT_BUMP);
    m_nh.getParam("/master_node/right_bump", RIGHT_BUMP);

    m_nh.getParam("/master_node/left_trig", LEFT_TRIG);
    m_nh.getParam("/master_node/right_trig", RIGHT_TRIG);
}

void ArmUs::send_cmd_motor()
{
    sensor_msgs::JointState msg;
    msg.name = { "motor1", "motor2" , "motor3", "motor4", "motor5" };
    msg.velocity = m_arm_us_info->MotorVelocities.get();
    m_pub_motor.publish(msg);
}

void ArmUs::send_cmd_motor_stop()
{
    sensor_msgs::JointState msg;
    msg.name = { "motor1", "motor2", "motor3", "motor4", "motor5" };
    msg.velocity = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    m_pub_motor.publish(msg);
    if (verbose)
    {
        ROS_WARN("All motors stopped");
    }
}

void ArmUs::send_gui_info()
{
    arm_us::GuiInfo msg;

    msg.position = m_arm_us_info->MotorPositions.get();
    msg.velocity = m_arm_us_info->MotorVelocities.get();
    msg.connected = m_arm_us_info->MotorConnections.get();
    msg.limit_reached = m_arm_us_info->MotorLimits.get();
    m_pub_gui.publish(msg);
}

void ArmUs::send_3d_graph_info()
{
    arm_us::GraphInfo msg;
    msg.angle = m_arm_us_info->JointAngles.get();
    m_pub_3d_graph.publish(msg);
}

bool ArmUs::call_inv_kin_calc_server()
{
    arm_us::InverseKinematicCalc srv;

    Vector5f angles = m_arm_us_info->JointAngles;
    srv.request.angles =  { angles.m1, angles.m2, angles.m3, angles.m4, angles.m5 };

    srv.request.commands[0] = m_arm_us_info->CartesianCommand.x;
    srv.request.commands[1] = m_arm_us_info->CartesianCommand.y;
    srv.request.commands[2] = m_arm_us_info->CartesianCommand.z;

    if (m_client_inv_kin_calc.call(srv))
    {
        if (srv.response.singularMatrix == false)
        {
            m_arm_us_info->MotorPositions.set(srv.response.velocities[0], srv.response.velocities[1], srv.response.velocities[2], srv.response.velocities[3], srv.response.velocities[4]);
        }
        else 
        {
            ROS_WARN("Singular Matrix");
        }
        return true;
    }
    else return false;
}