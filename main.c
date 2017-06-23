
// Include main header

#include "stm32f4xx.h"
#include "string.h"

// Include BSP Headers
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_sdram.h"
#include "stm32f429i_discovery_ioe.h"



// Include FreeRTOS headers
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"

// Include main header
#include "LTR_Ref.h"


int main(void)
{
	// Launch Trace recording
	vTraceInitTraceData();
	uiTraceStart();

	// Init
	STM_EVAL_LEDInit(LED3); // Green LED
	STM_EVAL_LEDInit(LED4); // Red LED

    // Start IO Extenders (Touchpad & PCF IO Extenders)
    IOE_Config();

    // Test if ITS-PLC I/O Extender is connected
    if (!(I2C_WritePCFRegister(PCF_B_WRITE_ADDR, (int8_t)0x00)))
    {
    	// I/O Extender not found, OFFLINE mode -> Red LED
    	STM_EVAL_LEDOff(LED3);
    	STM_EVAL_LEDOn(LED4);
    	return -1;
	}
	
	//Create Queue
	xQueueTEntree = xQueueCreate(10,sizeof(int)); // Task_E --> TEntree
	xQueuePortique = xQueueCreate(1,sizeof(int)); // Task_E --> Portique
	xQueueTSortie = xQueueCreate(10,sizeof(int)); // Task_E --> TSortie
	xQueueTask_E = xQueueCreate(10,sizeof(MSG)); // TEntree/Portique/TSortie --> Task_E
	xQueueTask_S = xQueueCreate(10,sizeof(MSG)); // TEntree/Portique/TSortie --> Task_S
	xQueueTEntree_Portique = xQueueCreate(10,sizeof(PIECE)); // TEntree --> Portique

	// Test Queues
	if( !(xQueueTEntree && xQueuePortique && xQueueTSortie && xQueueTask_E && xQueueTask_S && xQueueTEntree_Portique) )
    {
    	// not created -> Red LED
    	STM_EVAL_LEDOff(LED3);
    	STM_EVAL_LEDOn(LED4);
    	return -1;
    }

	// Create Semaphores
	vSemaphoreCreateBinary(xSemaphore_I2C);
	xSemaphore_nb_piece1 = xSemaphoreCreateCounting(3,3);
	xSemaphore_nb_piece2 = xSemaphoreCreateCounting(3,3);
	xSemaphore_nb_piece3 = xSemaphoreCreateCounting(3,3);

	vSemaphoreCreateBinary(xSemaphore_piece_presente);
	vSemaphoreCreateBinary(xSemaphore_piece_prise);
	vSemaphoreCreateBinary(xSemaphore_carton_pret);
	vSemaphoreCreateBinary(xSemaphore_carton_plein);

	xSemaphoreTake(xSemaphore_piece_presente,portMAX_DELAY);
	xSemaphoreTake(xSemaphore_piece_prise,portMAX_DELAY);
	xSemaphoreTake(xSemaphore_carton_pret,portMAX_DELAY);
	xSemaphoreTake(xSemaphore_carton_plein,portMAX_DELAY);

	// Test Semaphore creation
	if( (xSemaphore_I2C == NULL)|| (xSemaphore_piece_prise == NULL) || (xSemaphore_piece_presente == NULL) || (xSemaphore_carton_pret == NULL)|| (xSemaphore_carton_plein == NULL) )
    {
    	// not created -> Red LED
    	STM_EVAL_LEDOff(LED3);
    	STM_EVAL_LEDOn(LED4);
    	return -1;
    }		

	// Create Tasks
	xTaskCreate(vTEntree, "T_TEntree", 256, NULL, 1, NULL);		// priority 1
	xTaskCreate(vPortique, "T_Portique", 256, NULL, 2, NULL);	// priority 2
	xTaskCreate(vTSortie, "T_TSortie", 256, NULL, 3, NULL);		// priority 3
	xTaskCreate(vTask_E, "Task_E",256, NULL, 4, NULL);			// priority 4
	xTaskCreate(vTask_S, "Task_S", 256, NULL, 5, NULL);			// priority 5


	// Start scheduler
	vTaskStartScheduler();

	while(1)
    {
		// Never be here
    }


}


// Tapis entree
void vTEntree (void *pvParameters)
{
	static MSG Send_CMD;					// MSG Queue Send Task_E/S
	static PIECE Send_piece;				// Type and number piece
	static int S0_value = 0;				// Sensor 0 value
	static int S1_value = 0;				// Sensor 1 value
	static int S2_value = 0;				// Sensor 2 value
	static struct PIECE_TE piece ={0,3,3,3};// Structure definition piece Tapis entree
	static TEntree Etat_TEntree =  start;	// State tapis entree

	while(1)
	{
		switch (Etat_TEntree)
		{
			// Tapis entree ON
			case start:
				Send_CMD.CMD = A0;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_S, &Send_CMD , portMAX_DELAY);
				Etat_TEntree = wait_piece;
			break;

			// Wait type piece
			case wait_piece:
				do
				{	// Scrutation S0/S1
					Send_CMD.CMD = S0;
					Send_CMD.Value = 3;
					xQueueSend(xQueueTask_E,&Send_CMD, portMAX_DELAY);

					Send_CMD.CMD = S1;
					Send_CMD.Value = 3;
					xQueueSend(xQueueTask_E,&Send_CMD, portMAX_DELAY);

					xQueueReceive(xQueueTEntree,&S0_value, portMAX_DELAY);
					xQueueReceive(xQueueTEntree,&S1_value, portMAX_DELAY);

					// Wait
					vTaskDelay(500);

				} while((S0_value == 0) && (S1_value == 0));

				// Piece type 1 & free counting semaphore
				if ((S0_value==1)&&(S1_value==0)&&(xSemaphoreTake(xSemaphore_nb_piece1,0)))
				{
					// Recharge counter piece
					if(piece.cpt_piece1 == 0)
						piece.cpt_piece1 = 3;

					// Prepare MSG to Portique
					Send_piece.type = 1;
					Send_piece.cpt_piece = piece.cpt_piece1;

					piece.cpt_piece1--;
					Etat_TEntree = wait_s2;
				}

				// Piece type 2 & free counting semaphore
				else if ((S0_value==0)&&(S1_value==1)&&(xSemaphoreTake(xSemaphore_nb_piece2,0)))
				{
					// Recharge counter piece
					if(piece.cpt_piece2 == 0)
						piece.cpt_piece2 = 3;

					// Prepare MSG to Portique
					Send_piece.type = 2;
					Send_piece.cpt_piece = piece.cpt_piece2;

					piece.cpt_piece2--;
					Etat_TEntree = wait_s2;

				}

				// Piece type 3 & free counting semaphore
				else if ((S0_value==1)&&(S1_value==1)&&(xSemaphoreTake(xSemaphore_nb_piece3,0)))
				{
					// Recharge counter piece
					if(piece.cpt_piece3 == 0)
						piece.cpt_piece3 = 3;

					// Prepare MSG to Portique
					Send_piece.type = 3;
					Send_piece.cpt_piece =piece.cpt_piece3;

					piece.cpt_piece3--;
					Etat_TEntree = wait_s2;
				}
				break;

			// Wait sensor S2 Set
			case wait_s2 :
				Send_CMD.CMD = S2;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD , portMAX_DELAY);

				xQueueReceive(xQueueTEntree,&S2_value, portMAX_DELAY);

				// When S2 set, Tapis entree ON
				Send_CMD.CMD = A0;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_S,&Send_CMD, portMAX_DELAY);

				xQueueSend(xQueueTEntree_Portique, &Send_piece, portMAX_DELAY);

				// MA1 piece present
				xSemaphoreGive(xSemaphore_piece_presente);

				Etat_TEntree = wait_piece_prise;
			break;

			// Wait piece prise
			case wait_piece_prise:
				xSemaphoreTake(xSemaphore_piece_prise,portMAX_DELAY);
				Etat_TEntree =start;
			break;

			// Error state
			default :
				STM_EVAL_LEDOn(LED4);
				vTaskDelay(10);
			break;
		}
	}
}




// Portique
void vPortique (void *pvParameters)
{
	static MSG Send_CMD;			// MSG Queue Send Task_E / Task_S
	static int Receive = 0;			// Receive MSG Queue
	static PIECE Receive_piece;		// Type and number piece //{1,1}
	static int cpt_piece = 0;		// Number of piece in case
	static int i = 0;
	static TPortique State_Portique =  init_side;	// State of portique


	while(1)
	{
		switch(State_Portique)
		{
		// Init portique side position
		case init_side :
			for(i=0;i<3;i++)	//Update for(i=0;i<(4-Receive_piece.type);i++)
			{
				// Move to left
				Send_CMD.CMD = A3;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask start of Move to left
				Send_CMD.CMD = S5;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait S5 1
				xQueueReceive(xQueuePortique,&Receive,portMAX_DELAY);

				// Ask End of Move to left
				Send_CMD.CMD = S5;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait S5 0
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Reset Actuator
				Send_CMD.CMD = A3;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);
				vTaskDelay(100);
			}
			State_Portique = init_front;
		break;

		// Init portique front position
		case init_front:
			for(i=0;i<3;i++)	//Update for(i=0;i<(Receive_piece.cpt_piece-1);i++)
			{
				// Move to front
				Send_CMD.CMD = A4;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask start of Move to left
				Send_CMD.CMD = S5;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait S5 1
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Ask End of Move to left
				Send_CMD.CMD = S5;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait S5 0
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Reset Move to front
				Send_CMD.CMD = A4;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);
				vTaskDelay(100);
			}
			// Select first position sequence or not
			if(cpt_piece == 0)
				State_Portique = wait_piece_ready0;	// first (for first piece)
			else
				State_Portique = wait_piece_ready1;	// for other piece
				break;

		// Wait piece Tapis entree present & carton pret
		case wait_piece_ready0:
				// Wait Piece
				xSemaphoreTake(xSemaphore_piece_presente, portMAX_DELAY);

				// Wait Carton
				xSemaphoreTake(xSemaphore_carton_pret, portMAX_DELAY);

				State_Portique = take_piece;
				break;

		// Wait piece Tapis entree present
		case wait_piece_ready1:
				// Wait Piece
				xSemaphoreTake(xSemaphore_piece_presente, portMAX_DELAY);

				State_Portique = take_piece;
				break;

		// Take piece
		case take_piece:
				// Receive type and number piece from Tapis entree
				xQueueReceive(xQueueTEntree_Portique,&Receive_piece, portMAX_DELAY);

				// Go down
				Send_CMD.CMD = A6;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask down
				Send_CMD.CMD = S7;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait down
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);


				// Ventouse
				Send_CMD.CMD = A7;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask ventouse
				Send_CMD.CMD = S8;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait ventouse
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Go up
				Send_CMD.CMD = A6;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask up
				Send_CMD.CMD = S6;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait up
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);


				// MA1 piece prise
				xSemaphoreGive(xSemaphore_piece_prise);
				State_Portique = position_front;
				break;

		// Positionning front
		/**
		 *	-----------
		 * | 1 | 1 | 1 |
		 * |-----------|
		 * | 2 | 2 | 2 |
		 * |-----------|
		 * | 3 | 3 | 3 |
		 *  -----------
		 *
		 **/
		case position_front:

				// Repeat x type piece (1 2 3)
				for(i=0;i<(4-Receive_piece.type);i++)
				{
					// Move to back
					Send_CMD.CMD = A5;
					Send_CMD.Value = 1;
					xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

					// Ask start of Move to back
					Send_CMD.CMD = S5;
					Send_CMD.Value = 1;
					xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

					// Wait S5 1
					xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

					// Ask End of Move to back
					Send_CMD.CMD = S5;
					Send_CMD.Value = 0;
					xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

					// Wait S5 0
					xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

					// Reset Actuator
					Send_CMD.CMD = A5;
					Send_CMD.Value = 0;
					xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);
					vTaskDelay(100);
				}
				State_Portique = position_side;
				break;


		// Positionning side
		case position_side:
				// Repeat x numero piece (n°1 n°2 n°3)
				for(i=0;i<(Receive_piece.cpt_piece-1);i++)
				{
					// Move to left
					Send_CMD.CMD = A2;
					Send_CMD.Value = 1;
					xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

					// Ask start of Move to back
					Send_CMD.CMD = S5;
					Send_CMD.Value = 1;
					xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

					// Wait S5 1
					xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

					// Ask End of Move to back
					Send_CMD.CMD = S5;
					Send_CMD.Value = 0;
					xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

					// Wait S5 0
					xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

					// Reset Actuator
					Send_CMD.CMD = A2;
					Send_CMD.Value = 0;
					xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);
					vTaskDelay(100);
				}

				State_Portique = give_piece;
				break;

		// Release piece in box
		case give_piece:
				// Go down
				Send_CMD.CMD = A6;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask down
				Send_CMD.CMD = S7;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait down
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Realse piece
				Send_CMD.CMD = A7;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask ventouse disable
				Send_CMD.CMD = S8;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait ventouse
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Go up
				Send_CMD.CMD = A6;
				Send_CMD.Value = 0;
				xQueueSend(xQueueTask_S, &Send_CMD, portMAX_DELAY);

				// Ask up
				Send_CMD.CMD = S6;
				Send_CMD.Value = 1;
				xQueueSend(xQueueTask_E, &Send_CMD, portMAX_DELAY);

				// Wait up
				xQueueReceive(xQueuePortique,&Receive, portMAX_DELAY);

				// Counter of piece in box
				if(cpt_piece<8)
					cpt_piece++;	// each release piece, increment counter
				else
				{
					// MA1 carton plein
					xSemaphoreGive(xSemaphore_carton_plein);
					// Reset number of piece
					cpt_piece = 0;
				}
				State_Portique = init_side;
				break;

		// Erro state
		default :
			STM_EVAL_LEDOn(LED4);
			vTaskDelay(10);
		break;
		}
	}
}



// T_TSortie
void vTSortie (void *pvParameters)
{
	int capteur_S3=0;			// Sensor 3 value
	MSG Msg_sensor={0,0};		// MSG for Task_E (sensors)
	MSG Msg_actuator={0,0};		// MSG for Task_S (actuators)

	int Etat_Tapis_Sortie=1;	// Variable state Tapis sortie

	while(1)
	{

	   switch(Etat_Tapis_Sortie)
	   {
	   	case 1 :
	   			// Tapis sortie ON
	   			Msg_actuator.CMD=A1;
	   			Msg_actuator.Value=1;
				xQueueSend(xQueueTask_S, &Msg_actuator, portMAX_DELAY ); //actionne_moi A1 avance_tapis

				// Ask sensor 3 value (1)
				Msg_sensor.CMD=S3;
				Msg_sensor.Value=1;
				xQueueSend( xQueueTask_E, &Msg_sensor, portMAX_DELAY );

				//Wait Sensor 3 == 1
				xQueueReceive( xQueueTSortie, &(capteur_S3), portMAX_DELAY);
				// MA1 carton pret
				xSemaphoreGive(xSemaphore_carton_pret);
				Etat_Tapis_Sortie = 2;
		break;

		case 2:
				// Tapis sortie OFF
				Msg_actuator.CMD=A1;
				Msg_actuator.Value=0;
				xQueueSend(  xQueueTask_S, &Msg_actuator, portMAX_DELAY  );
				// Wait carton plein
				xSemaphoreTake(xSemaphore_carton_plein,portMAX_DELAY);
				Etat_Tapis_Sortie = 3 ;
		break;

///////////////////////////////////////////////////////////Attente_Reveil////////////////////////////////////////////////////////////////////////

		case 3 :
				// Tapis sortie ON
				Msg_actuator.CMD=A1;
				Msg_actuator.Value=1;
				xQueueSend( xQueueTask_S, &Msg_actuator, portMAX_DELAY ); 					//Avance_tapis

				Msg_sensor.CMD=S3;
				Msg_sensor.Value=0;
				xQueueSend( xQueueTask_E, &Msg_sensor, portMAX_DELAY );
				// Wait sensor 3 == 0
				xQueueReceive( xQueueTSortie, &capteur_S3, portMAX_DELAY);

				// Charge all counting semaphore with value '3'
				xSemaphoreGive(xSemaphore_nb_piece1);
				xSemaphoreGive(xSemaphore_nb_piece1);
				xSemaphoreGive(xSemaphore_nb_piece1);
				xSemaphoreGive(xSemaphore_nb_piece2);
				xSemaphoreGive(xSemaphore_nb_piece2);
				xSemaphoreGive(xSemaphore_nb_piece2);
				xSemaphoreGive(xSemaphore_nb_piece3);
				xSemaphoreGive(xSemaphore_nb_piece3);
				xSemaphoreGive(xSemaphore_nb_piece3);

				Etat_Tapis_Sortie = 1;
		break;
  	  }
	}
}


// Task_E (sensors)
void vTask_E (void *pvParameters)
{
	static uint16_t sensors = 0x0000;		// ITS Sensors value
	static MSG user_sensors_receive;		// MSG Queue Receive
											
	static MSG user_sensors_storage[16] = { // MSG Queue Storage
		{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},
		{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},
		{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},
		{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF},{0xFF,0xFF}};
	static int Send_SNS = 0;				// Send sensor 0 & 1 value to tapis entree only
	const int Send_CMD = 1;					// MSG Queue Send
	static int j = 0;						// Number of MSG execution
	portTickType TOP_Sample;				// Sample time


	while(1)
	{
		// TOP_Sample
		TOP_Sample = xTaskGetTickCount();

		// Read Queue until empty
		while(uxQueueMessagesWaiting(xQueueTask_E) != 0)
		{
			// Read 1 MSG from Queue
			if(xQueueReceive(xQueueTask_E,&user_sensors_receive,0))
			{
				// Store MSG in matching index
				switch (user_sensors_receive.CMD)
				{
				    case S0:
						user_sensors_storage[0] = user_sensors_receive;
				      break;

				    case S1:
						user_sensors_storage[1] = user_sensors_receive;
				      break;

				    case S2:
						user_sensors_storage[2] = user_sensors_receive;
				      break;

				    case S3:
						user_sensors_storage[3] = user_sensors_receive;
				      break;

				    case S4:
						user_sensors_storage[4] = user_sensors_receive;
				      break;

				    case S5:
						user_sensors_storage[5] = user_sensors_receive;
				      break;

				    case S6:
						user_sensors_storage[6] = user_sensors_receive;
				      break;

				    case S7:
						user_sensors_storage[7] = user_sensors_receive;
				      break;

				    case S8:
						user_sensors_storage[8] = user_sensors_receive;
				      break;
				}
			}
		}

		// Reserve ressource I2C
		xSemaphoreTake(xSemaphore_I2C, portMAX_DELAY);

		// Read I2C
		sensors = I2C_ReadPCFRegister(PCF_C_READ_ADDR) << 8;
		sensors = sensors | I2C_ReadPCFRegister(PCF_A_READ_ADDR);

		// free ressource
		xSemaphoreGive(xSemaphore_I2C);


		// if MSG available && Compare between sensors & MSG value
		for(j=0;j<16;j++)
		{
			// ONLY FOR TAPIS ENTREE
			// '3' is a neutral value
			if(user_sensors_storage[j].Value == 3)
			{
				// Send value sensors S0 & S1 value to tapis entree
				Send_SNS = sensors & S0;
				if(xQueueSend(xQueueTEntree,&Send_SNS, 0))
				{
					user_sensors_storage[0].CMD = 0xFF;
					user_sensors_storage[0].Value = 0xFF;
				}

				Send_SNS = (sensors & S1)>>1;
				if(xQueueSend(xQueueTEntree,&Send_SNS, 0))
				{
					user_sensors_storage[1].CMD = 0xFF;
					user_sensors_storage[1].Value = 0xFF;
				}
			}

			// For other task : send "1" only if Sensor asked (CMD) == asked Value (Value)
			else if ( ((sensors & user_sensors_storage[j].CMD)>>j) == user_sensors_storage[j].Value )
			{
				// Send MSG OK to matching Queue
				switch (user_sensors_storage[j].CMD)
				{

					case S2:
						if(xQueueSend(xQueueTEntree,&Send_CMD, 0))
						{
							user_sensors_storage[2].CMD = 0xFF;
							user_sensors_storage[2].Value = 0xFF;
						}
						break;

					case S3:
						if(xQueueSend(xQueueTSortie,&Send_CMD, 0))
						{
							user_sensors_storage[3].CMD = 0xFF;
							user_sensors_storage[3].Value = 0xFF;
						}
						break;

					case S4:
						if(xQueueSend(xQueuePortique,&Send_CMD, 0))
						{
							user_sensors_storage[4].CMD = 0xFF;
							user_sensors_storage[4].Value = 0xFF;
						}
						break;

					case S5:
						if(xQueueSend(xQueuePortique,&Send_CMD, 0))
						{
							user_sensors_storage[5].CMD = 0xFF;
							user_sensors_storage[5].Value = 0xFF;
						}
						break;

					case S6:
						if(xQueueSend(xQueuePortique,&Send_CMD, 0))
						{
							user_sensors_storage[6].CMD = 0xFF;
							user_sensors_storage[6].Value = 0xFF;
						}
						break;

					case S7:
						if(xQueueSend(xQueuePortique,&Send_CMD, 0))
						{
							user_sensors_storage[7].CMD = 0xFF;
							user_sensors_storage[7].Value = 0xFF;
						}
						break;

					case S8:
						if(xQueueSend(xQueuePortique,&Send_CMD, 0))
						{
							user_sensors_storage[8].CMD = 0xFF;
							user_sensors_storage[8].Value = 0xFF;
						}
						break;
				}
			}
		}

		// Wait delay sample
		vTaskDelayUntil(&TOP_Sample,10);
	}
}


// Task_S (actuators)
void vTask_S (void *pvParameters)
{
	static	uint8_t actuators = 0x00;		// ITS Actuator value
	static MSG user_actuators[10];			// MSG Queue
	int i = 0;								// Number of MSG available
	int j = 0;								// Number for execution

	while(1)
	{

		// Read Queue until empty
		do
		{
			// Read 1 MSG from Queue
			xQueueReceive(xQueueTask_S,&user_actuators[i],portMAX_DELAY);
			i++;

		}while(uxQueueMessagesWaiting(xQueueTask_S) != 0);


		// Cacul new actuator state
		for(j=0; j<i; j++)
		{
			// Actuator asked '1'
			if (user_actuators[j].Value == ON) // Ax = '1'
				actuators = actuators | user_actuators[j].CMD;
			else	// Ax = '0'
				actuators = (~user_actuators[j].CMD) & actuators;
		}

		// Reset Number of MSG
		i = 0;

		// Reserve ressource I2C
		xSemaphoreTake(xSemaphore_I2C, portMAX_DELAY);

		// Write I2C
		I2C_WritePCFRegister(PCF_B_WRITE_ADDR,actuators);

		// free ressource
		xSemaphoreGive(xSemaphore_I2C);
	}
}
