#include <ros/console.h>
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <kobuki_msgs/BumperEvent.h>
#include <sensor_msgs/LaserScan.h>
#include <eStop.h>



#include <nav_msgs/Odometry.h>
#include <tf/transform_datatypes.h>

#include <stdio.h>
#include <cmath>
#include <iostream>
#include <chrono>



using namespace std;

// global variables declarations
double angular = 0.0, linear = 0.0;
//odom variables
double posX=0, posY=0, yaw=0;
double pi=3.1416;
//bumper variables
bool bumperLeft = 0, bumperCenter = 0, bumperRight = 0;

//LaserScan variables
double laserRange = 10;
int laserSize = 0, laserOffset = 0, desiredAngle = 30;
int laserIndex;
double laserFront = 11;

bool leftTurn, rightTurn;
int dist_to_left, dist_to_right;

double left_most_dist, right_most_dist;
double mid_left_dist, mid_right_dist;
bool step1 = 1;// if step 1 is 1, run step 1; if step 1 is 0, run step 2
//sint dist_to_left, dist_to_right;
//double slam_map [][];
//double raw_map [][];
int bigturn_counter = 0;
// callback functions defined below
// each func receive a msg param with a specific variable type
// use global variable to share info back to main func (i.e. int main())


void bumperCallback(const kobuki_msgs::BumperEvent msg){

if (msg.bumper == 0)
	bumperLeft = !bumperLeft;
else if (msg.bumper == 1)
	bumperCenter = !bumperCenter;
else if (msg.bumper == 2)
	bumperRight = !bumperRight;
}


// LaserScan msg: http://docs.ros.org/melodic/api/sensor_msgs/html/msg/LaserScan.html
// Inside the laserCallback function calculate the size of the msg->ranges array. Also calculate the desired field of
// view. This parameter will be used to reduce the number of indices in the array that we will consider when
// finding a distance measurement.

void laserCallback(const sensor_msgs::LaserScan::ConstPtr& msg){
	//fill with your code
	laserSize = (msg->angle_max - msg->angle_min)/msg->angle_increment;
	laserOffset = desiredAngle*pi/(180*msg->angle_increment);
	ROS_INFO("Size of laser scan array: %i and size of offset: %i", laserSize, laserOffset);// Print the size of the array and the offset for testing purposes.

	// Below: distance measurement by searching the “ranges” array
	// done by finding the smallest distance in the desired field of view
	// The desired field of view is defined by a negative and positive laseroffset from the heading angle of the robot.
	//  If the desired field of view > sensor's, use sensor's instead

	laserRange = 11;
	laserFront = 11; 

	if (desiredAngle*pi/180 <msg->angle_max && -desiredAngle*pi/180 >msg->angle_min){
		for (int i =laserSize/2 - laserOffset; i<laserSize/2 + laserOffset; i++){
			if (laserRange > msg->ranges[i])
				laserRange = msg->ranges[i];
				laserIndex = i;
		}
	}

	else
	{
		for ( int i =0; i < laserSize; i++){
			if (laserRange>msg->ranges[i])
				laserRange = msg ->ranges[i];
				laserIndex = i;
		}
	}

	if (laserRange == 11){
		laserRange = 0;
	}

	if (laserIndex - laserSize/2 - laserOffset != laserIndex-laserSize/2 + laserOffset){
		if (laserIndex - laserSize/2 - laserOffset > laserIndex-laserSize/2 + laserOffset)
			leftTurn = !leftTurn;
		else
			rightTurn = !rightTurn;	
	}

	if (laserIndex <= 374 && laserIndex >= 264 && laserRange != 0) {
		laserFront = laserRange;
	}
		
}


//Odom Tutorial: http://wiki.ros.org/navigation/Tutorials/RobotSetup/Odom

void odomCallback(const nav_msgs::Odometry::ConstPtr& msg){
	posX = msg->pose.pose.position.x;
	posY = msg->pose.pose.position.y; 
	yaw = tf::getYaw(msg->pose.pose.orientation);  // convert queration to euler angles

	//ROS_INFO("Position: (%f, %f) Orientation: %f rad or %f degrees.", posX, posY, yaw, yaw*180/pi); // print pos and orientation for testing
}

void left_or_right() {
// decide left or right turn

	if (laserRange <= 1){
		if (laserIndex <= 264) {
			leftTurn = 1;
		}
		if (laserIndex >= 374) {
			rightTurn = 1;
		}
	}
}

int sgn(double v) {
	if (v < 0) return -1;
	if (v > 0) return 1;
	return 0;
}

void turn(double angle) {

	ros::NodeHandle nh;
	ros::Subscriber odom = nh.subscribe("odom", 1, odomCallback); 
	
 	//create a publisher named vel_pub; msg type is Twist; will send msg thru topic teleop;  size of the message queue is 1
	ros::Publisher vel_pub = nh.advertise<geometry_msgs::Twist>("cmd_vel_mux/input/teleop", 1); 

	geometry_msgs::Twist vel;

	ros::spinOnce();
	double yawGoal = yaw + angle;
	//double yawOld = yaw;
	if (yawGoal > pi) {
		yawGoal = yawGoal - pi*2;
	}
	if (yawGoal < -pi) {
		yawGoal = yawGoal + pi*2;
	}
	
	while (yaw <= yawGoal) {

		double angular = sgn(angle)*pi/6;
		double linear = 0;

		vel.angular.z = angular; 
		vel.linear.x = linear;
		vel_pub.publish(vel);
		
		ros::spinOnce();

	}
}

void straight () {
	angular = 0;
	linear = 0.1;
}

void smallTurn(int turnDirection){
	angular= sgn(turnDirection)*pi/6;
	linear=0.0;
}

void turn_or_straight (int turnDirection){
	// determine if the turtlebot need to do small turn, big turn or straight 
	if(laserRange > 1){
		//	ROS_INFO("SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS \n");
		straight();
	}

	if (laserFront <= 1 && laserFront != 0) { // turn left/right if face a wall
		turn(sgn(turnDirection)*0.5*pi);
		bigturn_counter++;
	}

	else if (rightTurn == 1 && laserFront > 1) { // slight right if too close to wall on the left
		smallTurn(-1);
	}

	else if (leftTurn == 1 && laserFront > 1) { // slight right if too close to wall on the left
		smallTurn(1);
	}
}



int main(int argc, char **argv)
{
	std::cout<<"enter main cout"<<std::endl;
	ROS_INFO ("entered main");
	ros::init(argc, argv, "image_listener");   //init a node called image_listener
	ros::NodeHandle nh; //
	teleController eStop;


	//create a subscriber named map_sub, subscrite to topic named map within gmapping, 
//	ros::Subscriber map_sub = nh.subscribe("map", 10, &mapCallback);


	//create a subscriber named bumper_sub; subscribe to topic named bumper; size of msg quene; ROS will call  bumperCallback() function whenever a new message arrives 
	ros::Subscriber bumper_sub = nh.subscribe("mobile_base/events/bumper", 10, &bumperCallback);
	ros::Subscriber laser_sub = nh.subscribe("scan", 10, &laserCallback);

	//image_transport::ImageTransport it(nh); //new

	//create a subscriber named bumper_sub; subscribe to topic named bumper; size of msg quene; ROS will call  bumperCallback() function whenever a new message arrives 
	// note: odom topic defined in turtlebot
	ros::Subscriber odom = nh.subscribe("odom", 1, odomCallback); 

 	//create a publisher named vel_pub; msg type is Twist; will send msg thru topic teleop;  size of the message queue is 1
	ros::Publisher vel_pub = nh.advertise<geometry_msgs::Twist>("cmd_vel_mux/input/teleop", 1); 

//	double angular = 0;
//	double linear = 0;

	geometry_msgs::Twist vel; //define a msg, type is Twist, name is vel

	std::chrono::time_point<std::chrono::system_clock> start;
	start  = std::chrono::system_clock::now(); //start timer
	uint64_t secondsElapsed = 0; //the timer just started, we know less than 480, no neded to change



	while(ros::ok() && secondsElapsed <=480 ){
	//	ROS_INFO("Position: (%f, %f) Orientation: %f laseRange: %f .", posX, posY, yaw*180/pi, laserRange); // print pos and orientation for testing

		ros::spinOnce();  // invoke all callback funcs and publish msgs
		//.....**E-STOP DO NOT TOUCH**.......
		eStop.block();

		left_or_right(); // determine if there is anything on the side;
		
		if (bigturn_counter <= 6){  // step 1
			turn_or_straight (-1); // right turn if face a wall
		}


		//ROS_INFO("mid_left_dist: %f, mid_right_dist %f \n", mid_left_dist, mid_right_dist);
		//ROS_INFO("angular is : %f, linear is : %f", angular, linear);
  		vel.angular.z = angular; 
  		vel.linear.x = linear;

  		vel_pub.publish(vel);   // vel_pub node publishes vel msg
		leftTurn = 0;
		rightTurn = 0;

		secondsElapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-start).count();
	}

	return 0;
}