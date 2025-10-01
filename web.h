#ifndef WEB_H
#define WEB_H

/***************** initWebServer ************************************************
 * params: none
 * return: void
 * Description:
 * Initializes a simple HTTP server to display sensor and control state.
 ******************************************************************************/
void initWebServer();

/***************** handleWebServer **********************************************
 * params: none
 * return: void
 * Description:
 * Handles incoming HTTP requests, must be called frequently in loop().
 ******************************************************************************/
void handleWebServer();

#endif
