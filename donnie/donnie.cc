/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2003  
 *     Brian Gerkey, Andrew Howard
 *                      
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * Desc: A simple example of how to write a driver that supports multiple interface.
 * Also demonstrates use of a driver as a loadable object.
 * Author: Andrew Howard
 * Date: 25 July 2004
 * CVS: $Id: Donnie.cc 8000 2009-07-12 10:53:35Z gbiggs $
 */


// ONLY if you need something that was #define'd as a result of configure 
// (e.g., HAVE_CFMAKERAW), then #include <config.h>, like so:
/*
#include <config.h>
*/

#if !defined (WIN32)
	#include <unistd.h>
	#include <netinet/in.h>
#endif

#include "protocol.cc"
#include "motors.h"
#include <string.h>
#include <libplayercore/playercore.h>

#include <iostream>
#include <sys/time.h>


#define BUFFER_SIZE 200


// Defines
#define PI 3.141592653
#define DONNIE_DIAMETER 0.44 //[m]
#define DONNIE_RADIUS DONNIE_DIAMETER * 0.5 //[m]
#define DONNIE_CIRCUMFERENCE 2 * PI * DONNIE_RADIUS //[m]
#define DONNIE_CENTRE_TO_WHEEL 0.135 //[m]
#define DONNIE_WHEEL_RADIUS 0.04 //[m]
#define PULSE_TO_RPM 1.83  //[rpm] (SEC_PR_MIN*MSEC_PR_SEC) / GEAR_RATIO / PULSES_PR_REV

////////////////////////////////////////////////////////////////////////////////
// The class for the driver
class Donnie : public ThreadedDriver{
	public:

		// Constructor; need that
		Donnie(ConfigFile* cf, int section);

		// This method will be invoked on each incoming message
		virtual int ProcessMessage(QueuePointer & resp_queue, player_msghdr * hdr, void * data);

	private:
		// Main function for device thread.
		virtual void Main();

		int processIncomingData(); //g

		void ProcessDioCommand(player_msghdr_t* hdr, player_dio_cmd_t &data);   
		void ProcessPos2dVelCmd(player_msghdr_t* hdr, player_position2d_cmd_vel_t &data);
		void ProcessPos2dPosCmd(player_msghdr_t* hdr, player_position2d_cmd_pos_t &data);
		void ProcessPos2dGeomReq(player_msghdr_t* hdr);


		void ProcessDioData();
		void ProcessRangerData();
		void ProcessBumperData();
		void ProcessPowerData();
		void ProcessSystemMessageData();
		void ProcessRequestConfig();
		void ProcessRequestPing();


		
	/*  
	Definition:

	player_dio_cmd_t struct: 
		uint32_t count; 
		uint32_t digout;

	player_dio_data:
		uint32_t count;
		uint32_t bits;

	Source: player-3.0.2/build/libplayerinterface/player_interfaces.h 
	*/
		std::string port;

		Serial *arduino;
		unsigned char rx_data[BUFFER_SIZE], tx_data[BUFFER_SIZE]; //g
		unsigned int rx_data_count, tx_data_count; //g
 

		// My dio interface
		player_devaddr_t m_dio_addr;  
		// My ranger interface
		player_devaddr_t m_ranger_addr;
		// My position interface
		player_devaddr_t m_position_addr;
		player_position2d_data_t m_pos_data;  
		// My bumper interface
		player_devaddr_t bumper_addr;
		// My power interface
		player_devaddr_t power_addr;

		//robot geometry members
		double m_width;
		double m_length;
		double m_height;

		//robot parameters 
		double k_linear_max_vel;
		double k_ang_max_vel;


		virtual int MainSetup();
		virtual void MainQuit();
};


// A factory creation function, declared outside of the class so that it
// can be invoked without any object context (alternatively, you can
// declare it static in the class).  In this function, we create and return
// (as a generic Driver*) a pointer to a new instance of this driver.
Driver* Donnie_Init(ConfigFile* cf, int section){
	// Create and return a new instance of this driver
	return ((Driver*) (new Donnie(cf, section)));
}

// A driver registration function, again declared outside of the class so
// that it can be invoked without object context.  In this function, we add
// the driver into the given driver table, indicating which interface the
// driver can support and how to create a driver instance.
void Donnie_Register(DriverTable* table){
	table->AddDriver("donnie", Donnie_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Extra stuff for building a shared object.

/* need the extern to avoid C++ name-mangling  */
extern "C"{
	int player_driver_init(DriverTable* table)
	{
		puts("Example arduino driver initializing");
		Donnie_Register(table);
		puts("Example arduino driver done");
		return(0);
	}
}
////////////////////////////////////////////////////////////////////////////////

// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
Donnie::Donnie(ConfigFile* cf, int section) : ThreadedDriver(cf, section){
	memset(&m_pos_data, 0, sizeof(player_position2d_data_t));  //descobrir o porque disso //g
  memset (&this->power_addr, 0, sizeof (player_devaddr_t));
  memset (&this->bumper_addr, 0, sizeof (player_devaddr_t));


	// Create dio interface
	 if (cf->ReadDeviceAddr(&(this->m_dio_addr), section, "provides", PLAYER_DIO_CODE, -1, NULL)){
			PLAYER_ERROR("Could not read dio ID ");
			SetError(-1);
			return;
	 }
	 if (AddInterface(this->m_dio_addr)){
			PLAYER_ERROR("Could not add dio interface ");
			SetError(-1);
			return;
	 }
	 // Create my ranger interface
	 if (cf->ReadDeviceAddr(&(this->m_ranger_addr), section, "provides", PLAYER_RANGER_CODE, -1, NULL)){
			PLAYER_ERROR("Could not read ranger ID ");
			SetError(-1);
			return;
	 }
	 if (AddInterface(this->m_ranger_addr)){
			PLAYER_ERROR("Could not add ranger interface ");
			SetError(-1);
			return;
	 }

		// Create my position interface
	 if (cf->ReadDeviceAddr(&(this->m_position_addr), section, "provides", PLAYER_POSITION2D_CODE, -1, NULL)){
			PLAYER_ERROR("Could not read position2d ID ");
			SetError(-1);
			return;
	 }
	 if (AddInterface(this->m_position_addr)){
			PLAYER_ERROR("Could not add position2d interface ");
			SetError(-1);    
			return;
	 }
	 // Create my bumper interface
		if (cf->ReadDeviceAddr (&(this->bumper_addr),section,"provides",PLAYER_BUMPER_CODE, -1, NULL)){
			PLAYER_ERROR("Could not read bumper interface ");
			SetError (-1);
		return;
		}
		if (AddInterface (this->bumper_addr) != 0){
			PLAYER_ERROR("Could not add bumper interface ");
			SetError (-1);
		return;
		}
		// Create my power interface
		if (cf->ReadDeviceAddr (&(this->power_addr),section,"provides",PLAYER_POWER_CODE,-1, NULL)){
      PLAYER_ERROR("Could not read power interface ");
      this->SetError (-1);
	  	return;
    }
    if (this->AddInterface (this->power_addr) != 0){
    	PLAYER_ERROR("Could not add power interface ");
	  	this->SetError (-1);
	  	return;
		}
	 
		port = cf->ReadString (section, "port", "/dev/ttyACM0");
		m_width = cf->ReadFloat(section, "width", 0.2);    // [m]
		m_length = cf->ReadFloat(section, "length", 0.2);  // [m]
		m_height = cf->ReadFloat(section, "height", 0.1);  // [m]

		k_linear_max_vel = cf->ReadFloat(section, "k_linear_max_vel", 10); 
		k_ang_max_vel = cf->ReadFloat(section, "k_ang_max_vel", 10);



	 //this->RegisterProperty ("port", &this->port, cf, section);
	 /*
		m_baud_rate = cf->ReadInt(section, "baud", 38400);
		m_quiet = cf->ReadBool(section, "quiet", TRUE);
		robot_geom.size.sw = cf->ReadTupleFloat(section, "robot_geometry", 0, 0.01);
		robot_geom.size.sl = cf->ReadTupleFloat(section, "robot_geometry", 1, 0.01);
		*/
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int Donnie::MainSetup(){   
	puts("MainSetup driver initialising");

	arduino = new Serial(port.c_str());  //c_str() convert string to const char*
 
	puts("MainSetup driver ready");
	return(0);
}


////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
void Donnie::MainQuit(){
	PLAYER_MSG0(0, "Shutting Donnie driver down...");

	 // Stop and join the driver thread
	 //StopThread();

		// delete the interface library
	 if (arduino)
	 {
			//if (motors)
			//{
			//   PLAYER_MSG0(0, "Stopping motors.\n");
			//   //motors->Coast();
			//}
			//motors = NULL;
			arduino->closePort();
			arduino = NULL;
	 }

	 //PLAYER_MSG0(0, "Donnie has been shutdown\n");
}


int Donnie::processIncomingData(){
	//uint8_t i;
	if(arduino->readData(rx_data,&rx_data_count)){
		if(rx_data[0]==DIOPACK) ProcessDioData();
		else if(rx_data[0]==RANGERPACK) ProcessRangerData();
		else if(rx_data[0]==BUMPERPACK) ProcessBumperData();
		else if(rx_data[0]==POWERPACK) ProcessPowerData();
		else if(rx_data[0]==SYSTEMMESSAGEPACK) ProcessSystemMessageData();
		else if(rx_data[0]==REQUESTCONFIGPACK) ProcessRequestConfig();
		else if(rx_data[0]==PINGPACK) ProcessRequestPing();
		else printf("unknown message, %.2X\n\n",rx_data[0]);
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Main function for device thread
void Donnie::Main(){
	// The main loop; interact with the device here
	for(;;){
		// test if we are supposed to cancel
		pthread_testcancel();

		
		processIncomingData(); //deal with the incoming data from driver. Determines what to do acording the message type

		// Process incoming messages.  Calls ProcessMessage() on each pending message.
		ProcessMessages();

		//give robot a chance to change state May this can lag the sonar update
		usleep(10); //Warning: This can lag sonar's readings
	}
	return;
}


int Donnie::ProcessMessage(QueuePointer & resp_queue, player_msghdr * hdr, void * data){
	 // Handle new data comming from client
	 PLAYER_WARN("New message received");
	 if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD, PLAYER_DIO_CMD_VALUES, m_dio_addr)){
			ProcessDioCommand(hdr, *reinterpret_cast<player_dio_cmd_t *>(data));
			
			return(0);
	 }
	 else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA, PLAYER_DIO_DATA_VALUES, m_dio_addr)){
			//ProcessDioData(hdr, *reinterpret_cast<player_dio_data_t *>(data));
			PLAYER_WARN("Dio data received");
			return(0);
	 }
	 else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD, PLAYER_POSITION2D_CMD_POS, m_position_addr)){
			//to use foo.GoTo (player_pose2d_t pos, player_pose2d_t vel)
			PLAYER_WARN("position2d goto cmd received");
			assert(hdr->size == sizeof(player_position2d_cmd_pos_t)); //g if this is false then call eception error
			ProcessPos2dPosCmd(hdr, *reinterpret_cast<player_position2d_cmd_pos_t *>(data));
			return(0);
	 }
	 else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD, PLAYER_POSITION2D_CMD_VEL, m_position_addr)){
			//to use foo.SetSpeed (double aXSpeed, double aYawSpeed)
			PLAYER_WARN("position2d vel cmd received");
			assert(hdr->size == sizeof(player_position2d_cmd_vel_t));
			ProcessPos2dVelCmd(hdr, *reinterpret_cast<player_position2d_cmd_vel_t *>(data));
			return(0);
	 }
	 else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, PLAYER_POSITION2D_REQ_MOTOR_POWER, m_position_addr)){
			//to use foo.SetMotorEnable(bool enable)
			PLAYER_WARN("position2d motor power cmd received");
			this->Publish(m_position_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_POSITION2D_REQ_MOTOR_POWER);
			return 0;
	 }
	 else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, PLAYER_POSITION2D_REQ_GET_GEOM, m_position_addr)){
			//to use foo.RequestGeom()
			PLAYER_WARN("position2d update geometry request received");
			ProcessPos2dGeomReq(hdr);
			return(0);
	 }

	

	// Tell the caller that you don't know how to handle this message
	return(-1);
}

void Donnie::ProcessDioCommand(player_msghdr_t* hdr, player_dio_cmd_t &data){
	 PLAYER_WARN("Message ProcessDioCommand");

	 std::cout << "Dio count:" << data.count << std::endl; //bits qnt
	 std::cout << "Dio digout:" << std::hex << data.digout << std::endl; //decimal value

	 tx_data_count=2;
	 tx_data[0]=DIOPACK;
	 tx_data[1]=(uint8_t)data.digout;
	 arduino->writeData(tx_data,tx_data_count);

	 //Publish a message via one of this driver's interfaces. This publish data to subscribed clients.
	 //This form of Publish will assemble the message header for you. The message is broadcast to all interested parties
	 Publish(m_dio_addr, PLAYER_MSGTYPE_DATA, PLAYER_DIO_CMD_VALUES, reinterpret_cast<void*>(&data), sizeof(data), NULL); //the NULL is the Timestamp and meens that the current time will be filled in) 
}

void Donnie::ProcessPos2dPosCmd(player_msghdr_t* hdr,
																			player_position2d_cmd_pos_t &data){
	std::cout << "Pos2DPosCmd pos.px:" << data.pos.px << " pos.py:" << data.pos.py << " pos.pa:"<< data.pos.pa << std::endl; //bits qnt
	std::cout << "Pos2DPosCmd vel.px:" << data.vel.px << " vel.py:" << data.vel.py << " vel.pa:"<< data.vel.pa << std::endl; //bits qnt
	std::cout << "Pos2DVelCmd state:" << data.state << std::endl;
	std::cout << std::endl;


}


double map(double x, double in_min, double in_max, double out_min, double out_max){
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


//foo.SetSpeed(double x, double pa)
void Donnie::ProcessPos2dVelCmd(player_msghdr_t* hdr, 
																			player_position2d_cmd_vel_t &data){
	//double linear_r, angular_r;
	double right_aux, left_aux,right_pwm, left_pwm;


	std::cout << "Pos2DVelCmd vel.px:" << data.vel.px << " vel.py:" << data.vel.py << " vel.pa:"<< data.vel.pa << std::endl; //bits qnt
	std::cout << "Pos2DVelCmd state:" << std::hex << data.state << std::endl;
	std::cout << std::endl;

	//linear_r = data.vel.px*k_linear_max_vel;
	//angular_r = data.vel.pa*k_ang_max_vel;

	//if(data.vel.px>maxSpeed) std::cout << "Valor acima da velocidade maxima: " << data.vel.px << std::endl;
	
	right_aux=data.vel.px + data.vel.pa;
	left_aux=data.vel.px - data.vel.pa;

	right_pwm = map(right_aux,0,k_linear_max_vel,0,255);
	left_pwm = map(left_aux,0,k_linear_max_vel,0,255);


	//std::cout << "right_pwm: " << right_pwm << std::endl;
 // std::cout << "left_pwm: " << left_pwm << std::endl;

	tx_data_count=4;
	tx_data[0]=MOTORPACK;
	if(right_pwm>0){
		tx_data[1]=0x0f;
		tx_data[2]=right_pwm;
	}
	else{
		tx_data[1]=(uint8_t)0xf0;
		tx_data[2]=(uint8_t)(-1*right_pwm);
	}
	if(left_pwm>0){
		tx_data[3]=0x0f;
		tx_data[4]=left_pwm;
	}
	else{
		tx_data[3]=(uint8_t)0xf0;
		tx_data[4]=(uint8_t)(-1*left_pwm);
	}

	std::cout << "right_pwm: " << (int)tx_data[2] << std::endl;
	std::cout << "left_pwm: " << (int)tx_data[4] << std::endl;


	//arduino->writeData(tx_data,tx_data_count);


}

void Donnie::ProcessPos2dGeomReq(player_msghdr_t* hdr){
	// this function updates the geometry and its data can be acessed by foo.GetOffset()
	player_position2d_geom_t geom;

	geom.pose.px = m_pos_data.pos.px;                                           // [m]
	geom.pose.py = m_pos_data.pos.py;                                           // [m]
	geom.pose.pz = m_pos_data.pos.pa;                                           // [rad]
	geom.size.sl = m_length;                                                    // [m]
	geom.size.sw = m_width;                                                     // [m]
	geom.size.sh = m_height;                                                    // [m]

	Publish(m_position_addr, 
				 PLAYER_MSGTYPE_RESP_ACK, PLAYER_POSITION2D_REQ_GET_GEOM, 
				 &geom, sizeof(geom), NULL);

}


void Donnie::ProcessDioData(){
	printf("DIOPACK:");

	for(int i=rx_data[1]-1;i>=0;i--) printf("%u",(rx_data[2] >> i) & 1); //show value as binary
	printf("\n\n");                 

	player_dio_data_t diodata;
	diodata.count=rx_data[1]; 
	diodata.bits=rx_data[2]; //bitfield

	Publish(m_dio_addr, PLAYER_MSGTYPE_DATA, PLAYER_DIO_DATA_VALUES,
									 reinterpret_cast<void *>(&diodata), sizeof(diodata), NULL); 
}


void Donnie::ProcessRangerData(){
	uint8_t i;
	printf("RANGERPACK:");
	for(i=0;i<rx_data_count-1;i++){
		printf("%.2X",rx_data[i+1]); //+1 devido a prosicao zero ser o typo da mensagem
	}
	printf("\n\n");

	player_ranger_data_range_t rangerdata;
	memset( &rangerdata, 0, sizeof(rangerdata) );

	// a sonar/IR type with one range per beam origin 
	rangerdata.ranges_count = rx_data[1]; //quantity of sensors in this package
	rangerdata.ranges = new double[rx_data[1]]; //alocate memory for the rangers
	for(i=0;i<rx_data[1];i++){
		rangerdata.ranges[i] = rx_data[i+2];
	}


	Publish(m_ranger_addr, PLAYER_MSGTYPE_DATA, PLAYER_RANGER_DATA_RANGE,
		 reinterpret_cast<void *>(&rangerdata), sizeof(rangerdata), NULL); 
}

void Donnie::ProcessBumperData(){
	uint8_t i;
	printf("BUMPERPACK:");
	for(i=0;i<rx_data_count-1;i++){
		printf("%.2X",rx_data[i+1]); //+1 devido a prosicao zero ser o typo da mensagem
	}
	printf("\n\n");

	player_bumper_data_t bumperdata;
	memset(&bumperdata,0,sizeof(bumperdata));

	// Update bumper data
	bumperdata.bumpers_count = rx_data[1]; //quantity of bumpers in this package
	bumperdata.bumpers = new uint8_t[rx_data[1]];
	for(i=0;i<rx_data[1];i++){
		bumperdata.bumpers[i] = rx_data[i+2];
	}

	this->Publish(bumper_addr,
		PLAYER_MSGTYPE_DATA, PLAYER_BUMPER_DATA_STATE,
		(void*)&bumperdata, sizeof(bumperdata), NULL);

	delete bumperdata.bumpers;
}

void Donnie::ProcessPowerData(){
	uint8_t i;
	printf("POWERPACK:");
	for(i=0;i<rx_data_count-1;i++){
		printf("%.2X",rx_data[i+1]); //+1 devido a prosicao zero ser o typo da mensagem
	}
	printf("\n\n");


	player_power_data_t powerdata;
  memset(&powerdata,0,sizeof(powerdata));


  int16_t aux = 0;
  aux = aux ^ rx_data[2]; //0000 & 0001 = 0001
  aux = (aux << 8) ^ rx_data[1];   //0001 & 0100 = 0101


  // Update power data
	powerdata.volts = (float)aux/1000; // rx_data[1];  //float [V]
	//powerdata.watts = voltage * current;
	powerdata.valid = (PLAYER_POWER_MASK_VOLTS | PLAYER_POWER_MASK_WATTS); //sem essa mascara o valor do proxy nao atualiza

  //powerdata.percent =;
  //powerdata.charging =;

	this->Publish(power_addr,
		PLAYER_MSGTYPE_DATA, PLAYER_POWER_DATA_STATE,
		(void*)&powerdata, sizeof(powerdata), NULL);
}

void Donnie::ProcessSystemMessageData(){
	uint8_t i;
	printf("SYSTEM MESSAGE:");
	for(i=0;i<rx_data_count-1;i++){
		printf("%c",rx_data[i+1]); //+1 devido a prosicao zero ser o typo da mensagem
	}
	printf("\n\n");

}

void Donnie::ProcessRequestConfig(){
	uint8_t i;
	printf("RECEIVED REQUEST CONFIG:");
	for(i=0;i<rx_data_count-1;i++){
		printf("%.2X",rx_data[i+1]); //+1 devido a prosicao zero ser o typo da mensagem
	}
	printf("\n\n");

	puts("Sending Arduino Config...");
	// update arduino config variables
	tx_data_count=2;
	tx_data[0]=CONFIGPACK;
	tx_data[1]=42;
	arduino->writeData(tx_data,tx_data_count);
}


void Donnie::ProcessRequestPing(){
	uint8_t i;
	printf("RECEIVED REQUEST PING:");
		for(i=0;i<rx_data_count-1;i++){
		printf("%.2X",rx_data[i+1]); //+1 devido a prosicao zero ser o typo da mensagem
	}
	printf("\n\n");

	tx_data_count=2;
	tx_data[0]=PINGPACK;
	tx_data[1]=43;
	arduino->writeData(tx_data,tx_data_count);
}