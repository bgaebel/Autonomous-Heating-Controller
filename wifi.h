#ifndef WIFI_H
#define WIFI_H

/***************** Function Prototypes ******************************************/
void initWifi();
void ensureWifi();
/***************** handleMdns ***************************************************
 * params: none
 * return: void
 * Description:
 * Keeps the mDNS responder responsive. Call this in loop().
 ******************************************************************************/
void handleMdns();


#endif
