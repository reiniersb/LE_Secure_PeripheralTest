
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#define dbgmsg(...) {NRF_LOG_INFO( __VA_ARGS__);\
										while (NRF_LOG_PROCESS() != false);}
/* Colored messages. Caller should not include their own \n
   Example: dbgcol(red, blk, "Hi %u", 100); */
#define UART_color_reset "\017"
#define UART_fg_blk      "\020"
#define UART_fg_red      "\021"
#define UART_fg_grn      "\022"
#define UART_fg_yel      "\023"
#define UART_fg_blu      "\024"
#define UART_fg_mag      "\025"
#define UART_fg_cya      "\026"
#define UART_fg_whi      "\027"
#define UART_bg_blk      "\030"
#define UART_bg_red      "\031"
#define UART_bg_grn      "\032"
#define UART_bg_yel      "\033"
#define UART_bg_blu      "\034"
#define UART_bg_mag      "\035"
#define UART_bg_cya      "\036"
#define UART_bg_whi      "\037"
#define dbgcol(fg, bg, fmt, ...)\
        dbgmsg(UART_fg_ ## fg UART_bg_ ## bg fmt "\017\n", ## __VA_ARGS__)

