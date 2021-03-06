#include "protocol.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>

int send_cmd(struct protocol * cmd, int fd) {
    int ret;
    syslog(LOG_DEBUG, "DEBUG: Sending render cmd(%i %s %i/%i/%i) with protocol version %i to fd %i\n", cmd->cmd, cmd->xmlname, cmd->z, cmd->x, cmd->y, cmd->ver, fd);
    if ((cmd->ver > 3) || (cmd->ver < 1)) {
        syslog(LOG_WARNING, "WARNING: Failed to send render cmd with unknown protocol version %i on fd %d\n", cmd->ver, fd);
        return -1;
    }
    switch (cmd->ver) { 
    case 1:
        ret = send(fd, cmd, sizeof(struct protocol_v1), 0); 
        break; 
    case 2:  
        ret = send(fd, cmd, sizeof(struct protocol_v2), 0); 
        break; 
    case 3: 
        ret = send(fd, cmd, sizeof(struct protocol), 0); 
        break; 
    }
    if ((ret != sizeof(struct protocol)) && (ret != sizeof(struct protocol_v2)) && (ret != sizeof(struct protocol_v1))) {
        syslog(LOG_WARNING, "WARNING: Failed to send render cmd on fd %i\n", fd);
        perror("send error");
    }
    return ret;
}

int recv_cmd(struct protocol * cmd, int fd,  int block) {
    int ret, ret2;
    memset(cmd,0,sizeof(*cmd));
    ret = recv(fd, cmd, sizeof(struct protocol_v1), block?MSG_WAITALL:MSG_DONTWAIT);
    if (ret < 1) {
      /* ------------------------------------------------------------------------------
       * Previously there was a "Failed to read cmd on fd" debug message here, but that
       * has been removed since it happend on every fd just before closing.
       *
       * For completeness "ret" was negative, 
       * so https://github.com/openstreetmap/mod_tile/issues/77#issuecomment-50244923
       * isn't actually the correct diagnosis.
       * ------------------------------------------------------------------------------ */
        return -1;
    } else if (ret < sizeof(struct protocol_v1)) {
        syslog(LOG_INFO, "DEBUG: Read incomplete cmd on fd %i", fd);
        return 0;
    }
    if ((cmd->ver > 3) || (cmd->ver < 1)) {
        syslog(LOG_WARNING, "WARNING: Failed to recieve render cmd with unknown protocol version %i\n", cmd->ver);
        return -1;
    }
    syslog(LOG_DEBUG, "DEBUG: Got incoming request with protocol version %i\n", cmd->ver); 
    switch (cmd->ver) {
    case 1:
        ret2 = 0;
        break;
    case 2: ret2 = recv(fd, ((void*)cmd) + sizeof(struct protocol_v1), sizeof(struct protocol_v2) - sizeof(struct protocol_v1), block?MSG_WAITALL:MSG_DONTWAIT);
        break; 
    case 3: ret2 = recv(fd, ((void*)cmd) + sizeof(struct protocol_v1), sizeof(struct protocol) - sizeof(struct protocol_v1), block?MSG_WAITALL:MSG_DONTWAIT);
        break; 
    }
    
    if ((cmd->ver > 1) && (ret2 < 1)) {
        syslog(LOG_WARNING, "WARNING: Socket prematurely closed: %i\n", fd); 
        return -1;
    }
    
    ret += ret2;
    
    if ((ret == sizeof(struct protocol)) || (ret == sizeof(struct protocol_v1)) || (ret == sizeof(struct protocol_v2))) {
        return ret;
    }
    syslog(LOG_WARNING, "WARNING: Socket read wrong number of bytes: %i -> %li, %li\n", ret, sizeof(struct protocol_v2), sizeof(struct protocol)); 
    return 0;
}
