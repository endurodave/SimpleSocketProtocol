// SPP on Arduino example 

#include "ssp.h"
#include "ssp_fault.h"

static void SspErrorHandler(SspErr err)
{
    SSP_TRACE_FORMAT("SspErrorHandler: %d\n", err);
}

static void SspCallbackSocket0(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    // Received data callback?
    if (type == SSP_RECEIVE)
    {
        // Recieved successfully?
        if (status == SSP_SUCCESS)
            SSP_TRACE_FORMAT("SSP_RECEIVE PORT1: %s", (char*)data);
    }
    // Send data callback?
    else if (type == SSP_SEND)
    {
        if (status == SSP_SUCCESS)
            SSP_TRACE("SSP_SEND PORT1 SUCCESS");
        else
            SSP_TRACE("SSP_SEND PORT1 FAIL");
    }
    else
    {
        SSP_TRACE("UNKNOWN PORT1");
    }
}

static void SspCallbackSocket1(UINT8 socketId, const void* data, UINT16 dataSize,
    SspDataType type, SspErr status, void* userData)
{
    // Received data callback?
    if (type == SSP_RECEIVE)
    {
        // Recieved successfully?
        if (status == SSP_SUCCESS)
            SSP_TRACE_FORMAT("SSP_RECEIVE PORT2: %s", (char*)data);
    }
    // Send data callback?
    else if (type == SSP_SEND)
    {
        if (status == SSP_SUCCESS)
            SSP_TRACE("SSP_SEND PORT2 SUCCESS");
        else
            SSP_TRACE("SSP_SEND PORT2 FAIL");
    }
    else
    {
        SSP_TRACE("UNKNOWN PORT2");
    }
}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
  Serial.print("Start\n");

  SSP_SetErrorHandler(SspErrorHandler);

  // Initialize the ports
  SSP_Init(SSP_PORT1);
  SSP_Init(SSP_PORT2);
  
  SspErr err;

  // Open two sockets
  err = SSP_OpenSocket(SSP_PORT1, 0); 
  err = SSP_OpenSocket(SSP_PORT2, 1);

  // Register for callbacks
  err = SSP_Listen(0, &SspCallbackSocket0, NULL);
  err = SSP_Listen(1, &SspCallbackSocket1, NULL);
}

// the loop function runs over and over again forever
void loop() {
  static int cntr = 0;
  char send[32];
  UINT16 sendArrSize[2];
  const void* sendArr[2];

  SspErr err;
  
  snprintf(send, 32, "CNTR=%d\0", cntr++);

  // Send data
  err = SSP_Send(0, 1, send, UINT16(strlen(send))+1); 
  err = SSP_Send(1, 0, send, UINT16(strlen(send))+1);

  sendArr[0] = "Hello ";
  sendArrSize[0] = (UINT16)strlen((char*)sendArr[0]);
  sendArr[1] = "World\0";
  sendArrSize[1] = (UINT16)strlen((char*)sendArr[1]) + 1;

  // Send data
  err = SSP_SendMultiple(1, 0, 2, sendArr, sendArrSize);

  do
  {
      // Call while there is send or receive data to process
      SSP_Process();
  } while (!SSP_IsRecvQueueEmpty(SSP_PORT1) ||
           !SSP_IsRecvQueueEmpty(SSP_PORT2) ||
           SSP_GetSendQueueSize(SSP_PORT1) != 0 ||
           SSP_GetSendQueueSize(SSP_PORT2) != 0);

  Serial.print("Loop\n");
}
