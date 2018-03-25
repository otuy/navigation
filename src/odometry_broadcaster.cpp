#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <tf/tf.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Pose2D.h>
#include <std_msgs/Float64.h>

class OdomertyBroadcaster
{
private:
	ros::NodeHandle n;

	// stuff to publish
	ros::Publisher odom_pub;
	tf::TransformBroadcaster odom_broadcaster;

	// stuff to subscribe to
	ros::Subscriber nav_pose_sub;
	ros::Subscriber base_odom_x_sub;
	ros::Subscriber base_odom_y_sub;
	ros::Subscriber base_odom_yaw_sub;

	ros::Timer timer;

	// local storage

	// last pose estimate that reported by odometry unit
	geometry_msgs::Pose2D last_pose;
	// current pose estimate that could be inaccurate.
	geometry_msgs::Pose2D current_pose;
	geometry_msgs::Twist last_twist;
	ros::Time last_pose_time;
	bool last_pose_updated = true;

	ros::Time last_x_time;
	ros::Time last_y_time;
	ros::Time last_yaw_time;

	nav_msgs::Odometry odom_msg;

	static constexpr double TimerInterval = 0.1;
	// expecting 0.1 second interval; 100 percent margin
	static constexpr double PoseTimeoutThreshold = 0.2;

public:
	OdomertyBroadcaster(void);

	void baseOdomXCallback(const std_msgs::Float64::ConstPtr &msg);
	void baseOdomYCallback(const std_msgs::Float64::ConstPtr &msg);
	void baseOdomYawCallback(const std_msgs::Float64::ConstPtr &msg);

	//void poseCallback(const geometry_msgs::PoseStamped::ConstPtr &pose);
	void navPoseCallback(const geometry_msgs::Pose2D::ConstPtr &msg);
	void timerCallback(const ros::TimerEvent &event);
};

OdomertyBroadcaster::OdomertyBroadcaster(void)
{
	this->odom_pub = n.advertise<nav_msgs::Odometry>("odom", 50);
	//this->pose_pub = n.advertise<geometry_msgs::PoseStamped>("pose", 50);
	//this->pose_sub = n.subscribe("pose", 20, &OdomertyBroadcaster::poseCallback, this);
	//this->nav_pose_sub = n.subscribe<geometry_msgs::Pose2D, OdomertyBroadcaster>("pose", 20, &OdomertyBroadcaster::navPoseCallback, this);

	this->base_odom_x_sub = n.subscribe<std_msgs::Float64, OdomertyBroadcaster>("base/odom/x", 20, &OdomertyBroadcaster::baseOdomXCallback, this);
	this->base_odom_y_sub = n.subscribe<std_msgs::Float64, OdomertyBroadcaster>("base/odom/y", 20, &OdomertyBroadcaster::baseOdomYCallback, this);
	this->base_odom_yaw_sub = n.subscribe<std_msgs::Float64, OdomertyBroadcaster>("base/odom/yaw", 20, &OdomertyBroadcaster::baseOdomYawCallback, this);

	// 10 Hz
	ros::Duration duration(TimerInterval);
	this->timer = n.createTimer(duration, &OdomertyBroadcaster::timerCallback, this);

	this->last_pose_time = ros::Time::now();
	this->last_pose.x = 0.0;
	this->last_pose.y = 0.0;
	this->last_pose.theta = 0.0;
}

void OdomertyBroadcaster::timerCallback(const ros::TimerEvent &event)
{
#if 0
	double dt = event.current_real.toSec() - last_pose_time.toSec();

	if(dt > PoseTimeoutThreshold)
	{
		// too late...
		last_twist.linear.x  = 0.0;
		last_twist.linear.y  = 0.0;
		last_twist.angular.z = 0.0;
	}
	else if(dt > TimerInterval)
	{
		// something is a bit off, but still it's expected; estimate
		current_pose.x     += last_twist.linear.x  * dt;
		current_pose.y     += last_twist.linear.y  * dt;
		current_pose.theta += last_twist.angular.z * dt;
	}
#endif

	//since all odometry is 6DOF we'll need a quaternion created from yaw
	geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(current_pose.theta);

	//first, we'll publish the transform over tf
	geometry_msgs::TransformStamped odom_trans;
	odom_trans.header.stamp = event.current_real;
	odom_trans.header.frame_id = "odom";
	odom_trans.child_frame_id = "base_link";

	odom_trans.transform.translation.x = current_pose.x;
	odom_trans.transform.translation.y = current_pose.y;
	odom_trans.transform.rotation = odom_quat;

	//send the transform
	odom_broadcaster.sendTransform(odom_trans);

	//next, we'll publish the odometry message over ROS
	nav_msgs::Odometry odom;
	odom.header.stamp = event.current_real;
	odom.header.frame_id = "odom";

	//set the position
	odom.pose.pose.position.x = current_pose.x;
	odom.pose.pose.position.y = current_pose.y;
	odom.pose.pose.orientation = odom_quat;

	//set the velocity
	odom.child_frame_id = "base_link";
	odom.twist.twist.linear.x = last_twist.linear.x;
	odom.twist.twist.linear.y = last_twist.linear.y;
	odom.twist.twist.angular.z = last_twist.angular.z;

	//publish the message
	odom_pub.publish(odom);
}

void OdomertyBroadcaster::baseOdomXCallback(const std_msgs::Float64::ConstPtr &msg)
{
	last_pose.x = current_pose.x;
	current_pose.x = -msg->data;
	last_twist.linear.x = (current_pose.x - last_pose.x) / (ros::Time::now() - last_x_time).toSec();
	last_x_time = ros::Time::now();
}

void OdomertyBroadcaster::baseOdomYCallback(const std_msgs::Float64::ConstPtr &msg)
{
	last_pose.y = current_pose.y;
	current_pose.y = -msg->data;
	last_twist.linear.y = (current_pose.y - last_pose.y) / (ros::Time::now() - last_y_time).toSec();
	last_y_time = ros::Time::now();
}

void OdomertyBroadcaster::baseOdomYawCallback(const std_msgs::Float64::ConstPtr &msg)
{
	last_pose.theta = current_pose.theta;
	current_pose.theta = msg->data;
	last_twist.angular.z = (current_pose.theta - last_pose.theta) / (ros::Time::now() - last_yaw_time).toSec();
	last_yaw_time = ros::Time::now();
}

void OdomertyBroadcaster::navPoseCallback(const geometry_msgs::Pose2D::ConstPtr &pose)
{
	ros::Time current_time = ros::Time::now();
	double dt = current_time.toSec() - last_pose_time.toSec();

	double dx, dy, dtheta;

	if(dt > PoseTimeoutThreshold)
	{
		last_twist.linear.x = 0.0;
		last_twist.linear.y = 0.0;
		last_twist.angular.z = 0.0;
	}
	else
	{
		double dx = pose->x - last_pose.x;
		double dy = pose->y - last_pose.y;
		double dtheta = pose->theta - last_pose.theta;

		last_twist.linear.x = dx / dt;
		last_twist.linear.y = dy / dt;
		last_twist.angular.z = dtheta / dt;
	}

	last_pose.x = pose->x;
	last_pose.y = pose->y;
	last_pose.theta = pose->theta;

	current_pose.x = pose->x;
	current_pose.y = pose->y;
	current_pose.theta = pose->theta;

	last_pose_time = current_time;

	last_pose_updated = true;
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "odometry_broadcaster");

  ROS_INFO("odometry_broadcaster node has started.");

  OdomertyBroadcaster *_odom = new OdomertyBroadcaster();

  //ros::Rate r(1.0);
  ros::spin();

  ROS_INFO("odometry_broadcaster has been terminated.");
}



