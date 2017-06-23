
// Define Port IO
#define PCF_A_WRITE_ADDR		   0x70
#define PCF_A_READ_ADDR			   0x71

#define PCF_B_WRITE_ADDR		   0x72
#define PCF_B_READ_ADDR			   0x73

#define PCF_C_WRITE_ADDR		   0x74
#define PCF_C_READ_ADDR			   0x75

#define PCF_D_WRITE_ADDR		   0x76
#define PCF_D_READ_ADDR			   0x77


// CMD : PORT A sensors 0 - 7
// CMD : PORT C sensors 8 - 15
#define S0	0x0001	// Type Piece		(TEntree)
#define S1	0x0002	// Type Piece		(TEntree)
#define S2	0x0004	// Present Piece	(TEntree)
#define S3	0x0008	// Present Carton	(TSortie)
#define S4	0x0010	// Init Portique 	(TPortique)
#define S5	0x0020	// Move Portique 	(TPortique)
#define S6	0x0040	// Haut Bras		(TPortique)
#define S7	0x0080	// Bas Bras			(TPortique)
#define S8	0x0100	// Ventouse Prise	(TPortique)
#define S9	0x0200	// 
#define S10	0x0400	// 
#define S11	0x0800	// 
#define S12	0x1000	// 
#define S13	0x2000	// 
#define S14	0x4000	// 
#define S15	0x8000	// 

// CMD : PORT B actuator 0-7
#define A0	0x01 	// Tapis Entree
#define A1	0x02 	// Tapis Sortie
#define A2	0x04 	// Bras vers Droite
#define A3	0x08 	// Bras vers Gauche
#define A4	0x10 	// Bras Avance
#define A5	0x20 	// Bras Recule
#define A6	0x40 	// Bras Descend(1)/Remonte(0)
#define A7	0x80 	// Ventouse Prise(1)/Lache(0)

// Value : ON/OFF
#define ON	1
#define OFF	0

// MSG format to communicate with Task_E (sensors) and Task_S(actuators)
typedef struct MSG MSG;
struct MSG
{
	int CMD;	// number Sensor / Actuator
	int Value;	// Sensor / Actuator value
};


// New type, Tapis entree Send Type piece and Numero piece to Portique
typedef struct PIECE PIECE;
struct PIECE
{
	int type;
	int cpt_piece;
};



// State of Tapis entree
typedef enum TEntree TEntree;
enum TEntree
{
	start,
	wait_piece,
	wait_s2,
	wait_piece_prise
};

// Structure definition piece Tapis entree
struct PIECE_TE
{
	int type_piece;
	int cpt_piece1;
	int cpt_piece2;
	int cpt_piece3;
};

// State of Portique
typedef enum TPortique TPortique;
enum TPortique
{
	init_side,
	init_front,
	wait_piece_ready0,
	wait_piece_ready1,
	take_piece,
	position_front,
	position_side,
	give_piece
};


// Tasks prototypes
void vTEntree(void *pvParameters); // TEntree
void vPortique(void *pvParameters); // Portique
void vTSortie(void *pvParameters); // TSortie
void vTask_E(void *pvParameters); // Task_E (sensors)
void vTask_S(void *pvParameters); // Task_S (actuators)


// Queues
xQueueHandle xQueueTEntree; // Task_E --> TEntree
xQueueHandle xQueuePortique; // Task_E --> Portique
xQueueHandle xQueueTSortie; // Task_E --> TSortie
xQueueHandle xQueueTask_E; // TEntree/Portique/TSortie  --> Task_E
xQueueHandle xQueueTask_S; // TEntree/Portique/TSortie  --> Task_S
xQueueHandle xQueueTEntree_Portique; // TEntree --> Portique

// Semaphore i2c
xSemaphoreHandle xSemaphore_I2C;

// Semaphore counting piece
xSemaphoreHandle xSemaphore_nb_piece1;
xSemaphoreHandle xSemaphore_nb_piece2;
xSemaphoreHandle xSemaphore_nb_piece3;

// Semaphore Events
xSemaphoreHandle xSemaphore_piece_presente;
xSemaphoreHandle xSemaphore_portique_pret;
xSemaphoreHandle xSemaphore_carton_pret;
xSemaphoreHandle xSemaphore_carton_plein;
xSemaphoreHandle xSemaphore_piece_prise;

