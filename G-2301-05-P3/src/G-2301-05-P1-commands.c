#include "../includes/G-2301-05-P1-socket.h"
#include "../includes/G-2301-05-P1-commands.h"
#include "../includes/G-2301-05-P1-tools.h"
#include "../includes/G-2301-05-P1-types.h"

/**
 * @brief a partir de una lista de canales, saca otra lista igual, pero donde 
 * los canales de donde es operador el usuario aparecen con una @ delante
 *
 * @param socket el socket del usuario a ver
 * @param listIn la lista de canales a ver
 * @param nChannels numero de canales en listIn
 * @param listOut lista con las @ en los canales en los que es operador 
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int parseChannelList(int socket, char* listIn, long int nChannels, char** listOut) {
    char* tok, *nick, *aux;
    long int retVal;

    if (listIn == NULL || listOut == NULL || nChannels <= 0)
        return logIntError(-1, "error @ parseChannelList -> invalid argument");

    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK)
        return logIntError(retVal, "error @ parseChannelList -> getNickFromSocket");
    *listOut = (char*) malloc(sizeof (char)*(strlen(listIn) + nChannels + 2));
    if (listOut == NULL) {
        free(nick);
        return logIntError(ERROR_MALLOC, "error @ parseChannelList -> malloc");
    }
    *listOut[0] = 0;
    tok = strtok(listIn, " ");
    if ((IRCTAD_GetUserModeOnChannel(tok, nick) & IRCUMODE_OPERATOR) == IRCUMODE_OPERATOR) {
        aux = (char*) malloc(sizeof (char)*(strlen(tok) + 3));
        if (aux == NULL) {
            free(nick);
            free(*listOut);
            listOut = NULL;
            return logIntError(ERROR_MALLOC, "error @ parseChannelList -> malloc (2)");
        }
        sprintf(aux, "@%s ", tok);
    } else {
        aux = (char*) malloc(sizeof (char)*(strlen(tok) + 2));
        if (aux == NULL) {
            free(nick);
            free(*listOut);
            listOut = NULL;
            return logIntError(ERROR_MALLOC, "error @ parseChannelList -> malloc (3)");
        }
        sprintf(aux, "%s ", tok);
    }
    strcat(*listOut, aux);
    free(aux);
    while ((tok = strtok(NULL, " ")) != NULL) {
        if ((IRCTAD_GetUserModeOnChannel(tok, nick) & IRCUMODE_OPERATOR) == IRCUMODE_OPERATOR) {
            aux = (char*) malloc(sizeof (char)*(strlen(tok) + 3));
            if (aux == NULL) {
                free(nick);
                free(*listOut);
                listOut = NULL;
                return logIntError(ERROR_MALLOC, "error @ parseChannelList -> malloc (2)");
            }
            sprintf(aux, "@%s ", tok);
        } else {
            aux = (char*) malloc(sizeof (char)*(strlen(tok) + 2));
            if (aux == NULL) {
                free(nick);
                free(*listOut);
                listOut = NULL;
                return logIntError(ERROR_MALLOC, "error @ parseChannelList -> malloc (2)");
            }
            sprintf(aux, "%s ", tok);
        }
        strcat(*listOut, aux);
        free(aux);
    }
    free(nick);
    return IRC_OK;
}

/**
 * @brief commando por defecto
 * envía un mensaje de error al cliente, indicando que el comando introducido es invalido
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandDefault(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//DEFAULT 0
{
    char *command, *prefix, *nick;
    long int retVal;
    prefix = SERVERNAME;
    if (getNickFromSocket(socket, &nick) != IRC_OK)
        return logIntError(-1, "error @ commandUnknown -> getNickFromSocket");
    if ((retVal = IRCMsg_ErrUnKnownCommand(&command, prefix, nick, strin)) != IRC_OK) {
        free(nick);
        return logIntError(retVal, "error @ commandUnknown -> IRCMsg_ErrUnKnownCommand");
    }
    if (send(socket, command, strlen(command), 0) == -1) {
        free(nick);
        free(command);
        return logIntError(-1, "error @ commandUnknown -> send");
    }
    free(command);
    free(nick);
    return IRC_OK;
}

/**
 * @brief commando nick
 * permite al usuario cambiar de nickname
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandNick(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//NICK 2
{
    char *prefix, *nick, *msg, *command, * userOr, *nickOr, *realOr;
    long int id = 0, retVal;

    prefix = nick = msg = userOr = nickOr = realOr = command = NULL;
    if ((retVal = IRCParse_Nick(strin, &prefix, &nick, &msg)) != IRC_OK) {
        IRC_MFree(3, &prefix, &nick, &msg);
        return logIntError(retVal, "error @ commandNick -> IRCParse_Nick");
    }
    free(prefix);

    if ((retVal = getNickFromSocket(socket, &nickOr)) != IRC_OK) {
        IRC_MFree(3, &prefix, &nick, &msg);
        return logIntError(retVal, "error @ commandNick -> getNickFromSocket");
    }
    if ((retVal = IRCTADUser_Set(id, userOr, nickOr, realOr, userOr, nick, realOr)) != IRC_OK) { //setear nickname
        IRC_MFree(4, &prefix, &nick, &nickOr, &msg);
        return logIntError(retVal, "error @ commandNick -> IRCTADUser_Set");
    }
    prefix = SERVERNAME;
    if ((retVal = IRCMsg_Nick(&command, prefix, msg, nick)) != IRC_OK) {
        IRC_MFree(4, &prefix, &nick, &nickOr, &msg);
        return logIntError(retVal, "error @ commandNick -> IRCMsg_Nick");
    }

    if ((retVal = send(socket, command, strlen(command), 0)) == -1) {
        IRC_MFree(4, &prefix, &nick, &nickOr, &msg);
        return logIntError(ERROR_SEND, "error @ commandNick -> send\n");
    }
    IRC_MFree(5, &command, &msg, &nick, &nickOr, &realOr);
    return IRC_OK;
}

/**
 * @brief commando mode
 * permite al usuario cambiar el modo un canal especificado
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandMode(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO MODE 5
{
    long int retVal = 0;
    char* prefix, *channeloruser, *mode, *nick, *command, *user;
    prefix = channeloruser = mode = nick = command = user = NULL;

    if ((retVal = IRCParse_Mode(strin, &prefix, &channeloruser, &mode, &user)) != IRC_OK)
        return logIntError(retVal, "error @ commandMode -> IRCParse_Mode\n");
    prefix = SERVERNAME;
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) {
        IRC_MFree(3, &channeloruser, &mode, &user);
        return logIntError(retVal, "error @ commandMode -> getNickFromSocket");
    }
    //cambio modo
    if ((retVal = IRCTAD_Mode(channeloruser, nick, mode)) != IRC_OK) {
        IRC_MFree(4, &nick, &channeloruser, &mode, &user);
        return logIntError(retVal, "error @ commandMode -> IRCTAD_Mode\n");
    }

    //nick es la password
    //si es del tipo con password, setea la password
    if ((strcmp(mode, "\\+k") == 0) && user != NULL) {

        if ((retVal = IRCTADChan_SetPassword(channeloruser, user)) != IRC_OK) {
            IRC_MFree(4, &nick, &channeloruser, &mode, &user);
            return logIntError(retVal, "error @ commandMode -> IRCTADChan_SetPassword\n");
        }
    }
    //creo y envio respuesta
    if ((retVal = IRCMsg_Mode(&command, prefix, channeloruser, mode, nick)) != IRC_OK) {
        IRC_MFree(4, &nick, &channeloruser, &mode, &user);
        return logIntError(retVal, "error @ commandMode -> IRCMsg_Mode\n");
    }
    if ((retVal = send(socket, command, strlen(command), 0)) == -1) {
        IRC_MFree(5, &command, &nick, &channeloruser, &mode, &user);
        return logIntError(retVal, "error @ commandNick -> send\n");
    }

    IRC_MFree(5, &command, &nick, &channeloruser, &mode, &user);
    return IRC_OK;
}

/**
 * @brief commando quit
 * permite al usuario abandonar el servidor
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandQuit(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//QUIT 7
{
    char *prefix, *msg, *nick, *command;
    prefix = msg = nick = command = NULL;
    long int retVal;
    if ((retVal = IRCParse_Quit(strin, &prefix, &msg)) != IRC_OK)
        return logIntError(retVal, "error @ commandQuit -> IRCParse_Quit");
    free(prefix);
    prefix = SERVERNAME;
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) {
        IRC_MFree(1, &msg);
        return logIntError(retVal, "error @ commandQuit -> getNickFromSocket");
    }
    IRCTAD_Quit(nick);
    if ((retVal = IRCMsg_Quit(&command, prefix, msg)) != IRC_OK) {
        IRC_MFree(2, &msg, &nick);
        return logIntError(retVal, "error @ commandQuit -> IRCMsg_Quit");
    }
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(3, &command, &msg, &nick);
        return logIntError(ERROR_SEND, "error @ commandQuit -> send\n");
    }
    IRC_MFree(3, &command, &msg, &nick);
    close(socket);

    return IRC_OK;
}

/**
 * @brief commando join
 * permite al usuario unirse a un canal. En caso de que se requiera una password para unirse,
 * el usuario debería incluirla en el comando tal y como se indica en el RFC. Tratar de unirse a un
 * canal inexistente, creará dicho canal
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandJoin(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO JOIN 9
{
    long int id = 0;
    long int retVal;
    char *prefix = NULL, *channel = NULL, *key = NULL, *msg = NULL;
    char *userOr = NULL, *nickOr = NULL, *realOr = NULL;
    char *host = NULL, *IP = NULL, *away = NULL, *command = NULL;
    long int actionTs, creationTs;
    char* nickp;
    //parseamos la cadena recibida
    if ((retVal = IRCParse_Join(strin, &prefix, &channel, &key, &msg)) != IRC_OK) {
        if (retVal == IRCERR_ERRONEUSCOMMAND) {
            free(prefix);
            prefix = SERVERNAME;
            getNickFromSocket(socket, &nickOr);
            if ((retVal = IRCMsg_ErrNeedMoreParams(&command, prefix, nickOr, strin)) != IRC_OK) {
                IRC_MFree(5, &nickOr, &channel, &key, &msg, &command);
                return logIntError(retVal, "error @ commandJoin -> IRCMsg_ErrNeedMoreParams");
            }
            if (send(socket, command, strlen(command), 0) == -1) {
                IRC_MFree(5, &nickOr, &channel, &key, &msg, &command);
                return logIntError(-1, "error @ commandJoin -> send\n");
            }
            IRC_MFree(5, &nickOr, &channel, &key, &msg, &command);
            return 0;
        } else
            return logIntError(retVal, "error @ IRCParse_Join\n");
    }
    free(prefix);
    prefix = SERVERNAME;
    //obtenemos datos del usuario que esta asociado a el socket. e.d. quien ha enviado el comando
    nickOr = NULL;
    retVal = IRCTADUser_GetData(&id, &userOr, &nickOr, &realOr, &host, &IP, &socket, &creationTs, &actionTs, &away);
    getNickFromSocket(socket, &nickp);
    if (retVal != IRC_OK) {
        IRC_MFree(10, &userOr, &nickOr, &realOr, &host, &IP, &away, &channel, &key, &msg, &nickp);
        return logIntError(retVal, "INVALID USER\n");
    }

    //intentamos unirnos al canal
    retVal = IRCTADChan_GetModeInt(channel);
    //en caso de que el canal no requiera contraseña o que la contraseña sea la correcta
    if (((retVal & IRCMODE_CHANNELPASSWORD) != IRCMODE_CHANNELPASSWORD) ||
            (key != NULL && (IRCTADChan_TestPassword(channel, key) == IRC_OK))) {
        retVal = IRCTAD_Join(channel, nickOr, "o", key);
        if (retVal != IRC_OK) {
            IRC_MFree(10, &userOr, &nickOr, &realOr, &host, &IP, &away, &channel, &key, &msg, &nickp);
            return logIntError(retVal, "error @ commandJoin -> IRCTAD_Join");
        }
        //en caso de unirnos correctamente, preparamos un mensaje de respuesta al cliente
        if ((retVal = IRCMsg_Join(&command, nickOr, NULL, NULL, channel)) != IRC_OK) {
            IRC_MFree(10, &userOr, &nickOr, &realOr, &host, &IP, &away, &channel, &key, &msg, &nickp);
            return logIntError(retVal, "error @ commandJoin -> IRCMsg_Join");
        }
    } else {
        if ((retVal = IRCMsg_ErrBadChannelKey(&command, prefix, nickOr, channel)) != IRC_OK) {
            IRC_MFree(10, &userOr, &nickOr, &realOr, &host, &IP, &away, &channel, &key, &msg, &nickp);
            return logIntError(retVal, "error @ commandJoin -> IRCMsg_ErrBadChannelKey");
        }
    }

    //enviamos el mensaje de respuesta
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(11, &command, &userOr, &nickOr, &realOr, &host, &IP, &away, &channel, &key, &msg, &nickp);
        return logIntError(-1, "error @ commandJoin -> send\n");
    }
    IRC_MFree(11, &command, &userOr, &nickOr, &realOr, &host, &IP, &away, &key, &channel, &msg, &nickp);

    return IRC_OK;
}

/**
 * @brief commando part
 * permite al usuario abandonar el canal indicado. Si se trata de abandonar un canal invalido
 * se le enviará un mensaje de notificación al cliente
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandPart(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO PART 10
{
    char *prefix, *channel, *msg, *nick, *command;
    long int retVal;
    if ((retVal = IRCParse_Part(strin, &prefix, &channel, &msg)) != IRC_OK)
        return logIntError(retVal, "error @ commandPart -> IRCParse_Part");
    free(prefix);
    prefix = SERVERNAME;
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) {
        IRC_MFree(2, &channel, &msg);
        return logIntError(retVal, "error @ commandPart -> getNickFromSocket");
    }
    if ((retVal = IRCTAD_Part(channel, nick)) != IRC_OK) {
        if (retVal == IRCERR_NOVALIDCHANNEL) {
            if ((retVal = IRCMsg_ErrNoSuchChannel(&command, prefix, nick, channel)) != IRC_OK) {
                IRC_MFree(3, &nick, &channel, &msg);
                return logIntError(retVal, "error @ commandPart -> IRCMsg_ErrNoSuchChannel");
            }
            if (send(socket, command, strlen(command), 0) == -1) {
                IRC_MFree(4, &command, &nick, &channel, &msg);
                return logIntError(-1, "error @ commandPart -> send\n");
            }
            IRC_MFree(4, &command, &nick, &channel, &msg);
            return IRC_OK;
        } else {
            IRC_MFree(3, &nick, &channel, &msg);
            return logIntError(retVal, "error @ commandPart -> IRCMsg_Part");
        }
    }
    if ((retVal = IRCMsg_Part(&command, prefix, channel, msg)) != IRC_OK) {
        IRC_MFree(3, &nick, &channel, &msg);
        return logIntError(retVal, "error @ commandPart -> IRC_MsgPart");
    }
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(4, &command, &nick, &channel, &msg);
        return logIntError(ERROR_SEND, "error @ commandPart -> send\n");
    }
    IRC_MFree(4, &command, &nick, &channel, &msg);

    return IRC_OK;
}

/**
 * @brief commando topic
 * permite al usuario consultar el topic de un canal especificado o bien cambiar el mismo
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandTopic(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TOPIC 11
{
    long int retVal, cmode, umode;
    char *channel, *topic, *prefix, *command = NULL;
    char *nick = NULL;
    //parseamos el comando que nos llega
    if ((retVal = IRCParse_Topic(strin, &prefix, &channel, &topic)) != IRC_OK)
        return logIntError(retVal, "error @ commandTopic -> IRCParse_Join\n");
    //el prefix que usaremos nosotros es el SERVERNAME
    free(prefix);
    prefix = SERVERNAME;
    //obtenemos el nick del usuario asociado a este socket. e.d. quien ha mandado el comando
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) {
        IRC_MFree(3, &prefix, &channel, &topic);
        return logIntError(retVal, "error @ commandTopic -> getNickFromSocket");
    }
    //en caso de que el usuario no haya especificado el topic en el comando, procedemos a informar al cliente del topic del canal
    if (topic == NULL) {
        //obtenemos el topic del canal
        if ((retVal = IRCTAD_GetTopic(channel, &topic)) != IRC_OK) {
            IRC_MFree(3, &nick, &channel, &topic);
            return logIntError(retVal, "error @ commandTopic -> IRCTAD_GetTopic");
        }
        //si el canal no tiene topic, preparamos la respuesta 331 correspondiente a NoTopic
        if (topic == NULL && (retVal = IRCMsg_RplNoTopic(&command, prefix, nick, channel)) != IRC_OK) {
            IRC_MFree(3, &nick, &channel, &topic);
            return logIntError(retVal, "error @ commandTopic -> IRCMsg_RplNoTopic");
        }//si el canal tiene topic, preparamos la respuesta 332 correspondiente con topic
        else if (topic != NULL && (retVal = IRCMsg_RplTopic(&command, prefix, nick, channel, topic)) != IRC_OK) {
            IRC_MFree(3, &nick, &channel, &topic);
            return logIntError(retVal, "error @ commandTopic -> IRCMsg_RplTopic");
        }
        //en caso de que el comando si que tuviera especificado el topic, procedemos a cambiar el topic del canal
    } else {
        //miramos si puede hacerlo
        cmode = (IRCMODE_TOPICOP & IRCTADChan_GetModeInt(channel));
        umode = (IRCUMODE_OPERATOR & IRCTAD_GetUserModeOnChannel(channel, nick));
        if ((cmode != IRCMODE_TOPICOP) || //mira si hay que ser operador para cambiarlo
                ((cmode == IRCMODE_TOPICOP) && (umode == IRCUMODE_OPERATOR))) { //mira si eres operador en caso de que haga falta

            //cambiamos el topic antiguo por el nuevo topic
            if ((retVal = IRCTAD_SetTopic(channel, nick, topic)) != IRC_OK) {
                IRC_MFree(3, &nick, &channel, &topic);
                return logIntError(retVal, "error @ commandTopic -> IRCTAD_SetTopic");
            }
            //preparamos el mensaje de respuesta correspondiente a cambiar el topic
            if ((retVal = IRCMsg_Topic(&command, prefix, channel, topic)) != IRC_OK) {
                IRC_MFree(3, &nick, &channel, &topic);
                return logIntError(retVal, "error @ commandTopic -> IRCMsgRplTopic (2)");
            }
        } else {//si no tiene permisos
            if ((retVal = IRCMsg_ErrChanOPrivsNeeded(&command, prefix, nick, channel)) != IRC_OK)
                return logIntError(retVal, "error @ commandTopic -> IRCMsg_ErrChanOPrivsNeeded");
        }
    }
    //enviamos el mensaje que hemos preparado previamente
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(4, &command, &nick, &channel, &topic);
        return logIntError(ERROR_SEND, "error @ commandTopic -> send\n");
    }
    IRC_MFree(4, &channel, &topic, &command, &nick);

    return IRC_OK;
}

/**
 * @brief commando mode
 * permite al usuario obtener la lista de nicknames de usuarios que pertenezcan a un canal
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandNames(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO NAMES 12
{
    char *prefix = NULL, *channel = NULL, *target = NULL, *userList = NULL, *nick = NULL;
    long int numberOfUsers, retVal;
    char *command = NULL;
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK)
        return logIntError(retVal, "error @ commandNames -> getNickFromSocket");

    if ((retVal = IRCParse_Names(strin, &prefix, &channel, &target)) != IRC_OK) {
        free(nick);
        return logIntError(retVal, "error @ commandNames -> IRCParse_Names");
    }

    free(prefix);
    prefix = SERVERNAME;

    if (channel != NULL) {
        retVal = IRCTAD_ListNicksOnChannel(channel, &userList, &numberOfUsers);
        if (retVal == IRCERR_NOVALIDCHANNEL) {
            if ((retVal = IRCMsg_RplEndOfNames(&command, prefix, nick, channel)) != IRC_OK) {
                IRC_MFree(3, &nick, &channel, &target);
                return logIntError(retVal, "error @ commandNames -> IRCMsg_RplEndOfNames");
            }
            if (send(socket, command, strlen(command), 0) == -1) {
                IRC_MFree(4, &command, &nick, &channel, &target);
                return logIntError(ERROR_SEND, "error @ commandNames -> send\n");
            }
            IRC_MFree(4, &command, &nick, &channel, &target);
            return IRC_OK;
        } else if (retVal != IRC_OK) {
            IRC_MFree(3, &nick, &channel, &target);
            return logIntError(retVal, "error @ commandNames -> IRCTAD_ListNicksOnChannel");
        }
        //TODO mirar getmodechar para type
        if ((retVal = IRCMsg_RplNamReply(&command, prefix, nick, "", channel, userList)) != IRC_OK) {
            IRC_MFree(4, &userList, &nick, &channel, &target);
            return logIntError(retVal, "error @ commandNames -> IRCMsg_RplNamReply");
        }
        if (send(socket, command, strlen(command), 0) == -1) {
            IRC_MFree(5, &command, &userList, &nick, &channel, &target);
            return logIntError(ERROR_SEND, "error @ commandNames -> send\n");
        }
        free(command);
        command = NULL;
        if ((retVal = IRCMsg_RplEndOfNames(&command, prefix, nick, channel)) != IRC_OK) {
            IRC_MFree(5, &command, &userList, &nick, &channel, &target);
            return logIntError(retVal, "error @ commandNames -> IRCMsg_RplEndOfNames");
        }
        if (send(socket, command, strlen(command), 0) == -1) {
            IRC_MFree(5, &command, &userList, &nick, &channel, &target);
            return logIntError(ERROR_SEND, "error @ commandNames -> send\n");
        }
    }
    IRC_MFree(5, &command, &userList, &nick, &channel, &target);
    return IRC_OK;
}

/**
 * @brief commando list
 * permite al usuario obtener la lista de canales del servidor o bien información sobre un canal en partiular
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandList(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO LIST 13
{
    char **list, *prefix, *channel, *target, *nick, *topic, *visible;
    long num, retVal;
    char *command;
    int i;

    prefix = channel = target = nick = topic = visible = NULL; //iniciamos a NULL para la llamada a GetData
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) //obtenemos el nick a partir del socket
        logIntError(retVal, "error @ commandList -> getNickFromSocket");

    //parseamos el comando list
    if ((retVal = IRCParse_List(strin, &prefix, &channel, &target)) != IRC_OK) {
        free(nick);
        return logIntError(retVal, "error @ commandList -> IRCParse_List");
    }
    //nuestro prefix no es el que leemos, sino el nombre de nuestro server
    free(prefix);
    prefix = SERVERNAME;
    //obtenemos la lista de canales
    if ((retVal = IRCTADChan_GetList(&list, &num, NULL)) != IRC_OK) {
        IRC_MFree(3, &nick, &channel, &target);
        return logIntError(retVal, "error @ commandList -> IRCTADChan_GetList");
    }
    //preparamos el mensaje de respuesta 321 que indica el comienzo de la lista de canales
    if ((retVal = IRCMsg_RplListStart(&command, prefix, nick)) != IRC_OK) {
        IRCTADChan_FreeList(list, num);
        IRC_MFree(3, &nick, &channel, &target);
        return logIntError(retVal, "error @ commandList -> IRCMsg_RplListStart");
    }
    //enviamos el mensaje 321 de respuesta 
    if (send(socket, command, strlen(command), 0) == -1) {
        IRCTADChan_FreeList(list, num);
        IRC_MFree(4, &command, &nick, &channel, &target);
        return logIntError(ERROR_SEND, "error @ commandList -> send\n");
    }
    //en caso de que en el comando no se haya especificado el channel a mostrar
    if (channel == NULL) {
        //obtendremos todos los canales existentes en el servidor, recorriendo la lista
        for (i = 0; i < num; i++) {
            //obtenemos el modo del canal
            visible = IRCTADChan_GetModeChar(list[i]);
            if (strcmp(visible, "s") != 0) {
                //obtenemos el topic del canal de esta iteracion
                if ((retVal = IRCTAD_GetTopic(list[i], &topic)) != IRC_OK) {
                    IRCTADChan_FreeList(list, num);
                    IRC_MFree(4, &target, &nick, &visible, &command);
                    return logIntError(retVal, "error @ commandList -> IRCTAD_GetTopic");
                }
                //si el topic es NULL (porque no hay) diremos que tiene de topic una cadena vacía
                if (topic == NULL) {
                    topic = (char*) malloc(sizeof (char)*1);
                    strcpy(topic, "");
                }
                //preparamos el mensaje de respuesta 322 del comando list para el canal de esta iteración
                IRC_MFree(1, &command);
                if ((retVal = IRCMsg_RplList(&command, prefix, nick, list[i], visible, topic)) != IRC_OK) {
                    IRCTADChan_FreeList(list, num);
                    IRC_MFree(5, &target, &nick, &visible, &topic, &command);
                    return logIntError(retVal, "error @ commandList -> IRCMsg_RplList");
                }
                //enviamos el mensaje de respuesta 322 correspondiente al canal de esta iteracion
                if (send(socket, command, strlen(command), 0) == -1) {
                    IRCTADChan_FreeList(list, num);
                    IRC_MFree(5, &target, &nick, &visible, &topic, &command);
                    return logIntError(ERROR_SEND, "error @ commandList -> send\n");
                }
            }
            IRC_MFree(3, &command, &topic, &visible);
        }
    } else//en caso de que en el comando si que se especificara el canal a mirar:
    {
        //obtenemos el modo del canal
        visible = IRCTADChan_GetModeChar(channel);
        //obtenemos el topic del canal especificado
        if ((retVal = IRCTAD_GetTopic(channel, &topic)) != IRC_OK) {
            IRCTADChan_FreeList(list, num);
            IRC_MFree(4, &target, &nick, &command, &visible);
            return logIntError(retVal, "error @ commandList -> IRCTAD_GetTopic");
        }
        //si el topic es NULL (porque no hay) diremos que tiene de topic una cadena vacía
        if (topic == NULL) {
            topic = (char*) malloc(sizeof (char)*1);
            strcpy(topic, "");
        }
        //preparamos el mensaje de respuesta 322 del comando list para este canal
        IRC_MFree(1, &command);
        if ((retVal = IRCMsg_RplList(&command, prefix, nick, channel, visible, topic)) != IRC_OK) {
            IRCTADChan_FreeList(list, num);
            IRC_MFree(5, &target, &nick, &command, &visible, &topic);
            return logIntError(retVal, "error @ commandList -> IRCMsg_RplList");
        }
        //enviamos el mensaje de respuesta 322 del comando list para este canal
        if (send(socket, command, strlen(command), 0) == -1) {
            IRCTADChan_FreeList(list, num);
            IRC_MFree(5, &target, &nick, &command, &visible, &topic);
            return logIntError(ERROR_SEND, "error @ commandList -> send\n");
        }
        IRC_MFree(1, &channel);
    }
    //finalmente, preparamos el mensaje de respuesta 323 indicando el final de las respuestas del comando list
    if ((retVal = IRCMsg_RplListEnd(&command, prefix, nick)) != IRC_OK) {
        IRCTADChan_FreeList(list, num);
        IRC_MFree(2, &target, &nick);
        return logIntError(retVal, "error @ commandList -> IRCMsg_RplListEnd");
    }
    //enviamos el mensaje de respuesta 323 indicando el final de las respuestas del comando list
    if (send(socket, command, strlen(command), 0) == -1) {
        IRCTADChan_FreeList(list, num);
        IRC_MFree(3, &target, &nick, &command);
        return logIntError(ERROR_SEND, "error @ commandList -> send\n");
    }
    IRCTADChan_FreeList(list, num);
    IRC_MFree(3, &nick, &command, &target);

    return IRC_OK;
}

/**
 * @brief commando kick
 * permite al usuario expulsar a un usuario de un canal (siempre y cuando se sea operador del canal)
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandKick(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO KICK 15
{
    int socket2;
    char *prefix, *channel, *nick, *targetNick, *comment, *command;
    long int retVal;
    prefix = channel = nick = targetNick = comment = command = NULL;
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK)
        return logIntError(retVal, "error @ commandKick -> getNickFromSocket");
    if ((retVal = IRCParse_Kick(strin, &prefix, &channel, &targetNick, &comment)) != IRC_OK) {
        free(nick);
        return logIntError(retVal, "error @ commandKick -> IRCParse_Kick\n");
    }
    retVal = IRCTAD_GetUserModeOnChannel(channel, nick) & IRCUMODE_OPERATOR;
    free(prefix);
    prefix = SERVERNAME;
    if (retVal == IRCUMODE_OPERATOR) {
        if ((retVal = IRCTAD_KickUserFromChannel(channel, targetNick)) != IRC_OK) {
            IRC_MFree(4, &nick, &channel, &targetNick, &comment);
            return logIntError(retVal, "error @ commandKick -> IRCTAD_KickUserFromChannel\n");
        }

        if ((retVal = getSocketFromNick(&socket2, targetNick)) != IRC_OK) {
            IRC_MFree(4, &nick, &channel, &targetNick, &comment);
            return logIntError(retVal, "error @ commandKick -> getSocketFromNick");
        }
        if ((retVal = IRCMsg_Kick(&command, prefix, channel, targetNick, comment)) != IRC_OK) {
            IRC_MFree(4, &nick, &channel, &targetNick, &comment);
            return logIntError(retVal, "error @ commandKick -> IRCMsg_Kick\n");
        }

        if (send(socket2, command, strlen(command), 0) == -1) {
            IRC_MFree(5, &command, &nick, &channel, &targetNick, &comment);
            return logIntError(ERROR_SEND, "error @ commandKick -> send\n");
        }

    } else {
        if ((retVal = IRCMsg_ErrChanOPrivsNeeded(&command, prefix, nick, channel)) != IRC_OK) {
            IRC_MFree(4, &nick, &channel, &targetNick, &comment);
            return logIntError(retVal, "error @ commandKick -> IRCMsg_ErrChanOPrivsNeeded\n");
        }
        if (send(socket, command, strlen(command), 0) == -1) {
            IRC_MFree(5, &command, &nick, &channel, &targetNick, &comment);
            return logIntError(ERROR_SEND, "error @ commandKick -> send\n");
        }
    }
    IRC_MFree(5, &command, &nick, &channel, &targetNick, &comment);
    return IRC_OK;
}

/**
 * @brief commando Privmsg
 * permite al usuario enviar mensajes tanto a un usuario en concreto como a todos los usuarios de un canal
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandPrivmsg(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO PRIVMSG 16
{
    char *prefix, *msgtarget, *msg, *command, **userList, *nick, *away;
    long int id, creationTs, actionTs, numberUsers, retVal;
    int oSocket = 0, i;
    char *oName, *oReal, *oHost, *oIP, *oAway;

    //ponemos estos campos a NULL y a 0 para llamar luego a GetData
    prefix = msgtarget = msg = command = nick = away = oName = oReal = oHost = oIP = oAway = NULL;
    userList = NULL;
    id = creationTs = actionTs = 0;

    //parseamos el comando recibido
    if ((retVal = IRCParse_Privmsg(strin, &prefix, &msgtarget, &msg)) != IRC_OK)
        return logIntError(retVal, "error @ commandPrivmsg -> IRCParse_Privmsg");
    //el prefix que usaremos nosotros es SERVERNAME
    free(prefix);
    prefix = SERVERNAME;
    //preparamos en command el mensaje a enviarle al destino "target"
    if ((retVal = IRCMsg_Privmsg(&command, prefix, msgtarget, msg)) != IRC_OK) {
        IRC_MFree(2, &msgtarget, &msg);
        return logIntError(retVal, "error @ commandPrivmsg -> IRCMsg_Privmsg");
    }
    //en el caso en el que el mensaje NO sea para un canal
    if (msgtarget[0] != '#') {
        //comprobamos que el nick pertenece a algun usuario existente
        if (IRCTADUser_Test(0, NULL, msgtarget) != IRC_OK) {
            //obtenemos el nick del usuario que envio este mensaje
            if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) {
                IRC_MFree(3, &command, &msgtarget, &msg);
                return logIntError(retVal, "error @ commandPrivmsg -> getNickFromSocket");
            }
            //preparamos el mensaje de error que responder
            IRC_MFree(1, &command);
            if ((retVal = IRCMsg_ErrNoSuchNick(&command, prefix, nick, msgtarget)) != IRC_OK) {
                IRC_MFree(3, &nick, &msgtarget, &msg);
                return logIntError(retVal, "error @ commandPrivmsg -> getNickFromSocket");
            }
            //enviamos el mensaje de error
            if (send(socket, command, strlen(command), 0) == -1) {
                IRC_MFree(4, &command, &msgtarget, &msg, &nick);
                return logIntError(ERROR_SEND, "error @ commandPrivmsg -> send");
            }
            IRC_MFree(4, &command, &msgtarget, &msg, &nick);
            return IRC_OK;
        }
        //comprobamos que el otro usuario no esta away
        if ((retVal = getAwayFromSocket(socket, &away)) != IRC_OK) {
            IRC_MFree(4, &command, &msgtarget, &msg, &away);
            return logIntError(retVal, "error @ commandPrivmsg -> getAwayFromSocket");
        }
        if (away != NULL) {
            if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK) {
                IRC_MFree(3, &command, &msgtarget, &msg);
                return logIntError(retVal, "error @ commandPrivmsg -> getNickFromSocket");
            }
            IRC_MFree(1, &command);
            if ((retVal = IRCMsg_RplAway(&command, prefix, nick, msgtarget, away)) != IRC_OK) {
                IRC_MFree(5, &command, &msgtarget, &msg, &away, &nick);
                return logIntError(retVal, "error @ commandPrivmsg -> IRCMsg_RplAway");
            }
            if (send(socket, command, strlen(command), 0) == -1) {
                IRC_MFree(5, &command, &msgtarget, &msg, &away, &nick);
                return logIntError(ERROR_SEND, "error @ commandPrivmsg -> send (3)");
            }
            IRC_MFree(5, &command, &msgtarget, &msg, &away, &nick);
            return IRC_OK;
        }
        //obtenemos los datos del usuario al que se desea enviar el mensaje privado
        if ((retVal = IRCTADUser_GetData(&id, &oName, &msgtarget, &oReal, &oHost, &oIP, &oSocket, &creationTs, &actionTs, &oAway)) != IRC_OK) {
            IRC_MFree(3, &command, &msgtarget, &msg);
            return logIntError(retVal, "error @ commandPrivmsg -> IRCTADUser_GetData");
        }
        //enviamos el mensaje al usuario destino
        if (send(oSocket, command, strlen(command), 0) == -1) {
            IRC_MFree(8, &command, &msgtarget, &msg, &oName, &oReal, &oHost, &oIP, &oAway);
            return logIntError(ERROR_SEND, "error @ commandPrivmsg -> send (2)");
        }
    } else //en caso de que el mensaje sea para un canal
    {
        //obtenemos la lista de usuarios de dicho canal
        if ((retVal = IRCTAD_ListNicksOnChannelArray(msgtarget, &userList, &numberUsers)) != IRC_OK) {
            IRC_MFree(3, &command, &msgtarget, &msg);
            return logIntError(retVal, "error @ commandPrivmsg -> IRCTAD_ListNicksOnChannelArray");
        }
        //recorreremos la lista de nicks para enviar a cada uno de ellos el mensaje que se envia al canal
        IRC_MFree(1, &msgtarget);
        for (i = 0; i < numberUsers; i++) {
            //ponemos estos campos a NULL y a 0 porque habrán sido modificados en la iteración anterior
            //los necesitamos para cada llamada a GetData
            oName = oReal = oHost = oIP = oAway = NULL;
            id = creationTs = actionTs = oSocket = 0;
            //obtenemos los datos del usuario de esta iteración gracias a su nick
            if ((retVal = IRCTADUser_GetData(&id, &oName, userList + i, &oReal, &oHost, &oIP, &oSocket, &creationTs, &actionTs, &oAway)) != IRC_OK) {
                IRCTADUser_FreeList(userList, numberUsers);
                IRC_MFree(8, &command, &msgtarget, &msg, &oName, &oReal, &oHost, &oIP, &oAway);
                return logIntError(retVal, "error @ commandPrivmsg -> IRCTADUser_GetData");
            }
            //enviamos el mensaje al usuario de esta iteración
            if (send(oSocket, command, strlen(command), 0) == -1) {
                IRCTADUser_FreeList(userList, numberUsers);
                IRC_MFree(8, &command, &msgtarget, &msg, &oName, &oReal, &oHost, &oIP, &oAway);
                return logIntError(ERROR_SEND, "error @ commandPrivmsg -> send (3)");
            }
            IRC_MFree(5, &oName, &oReal, &oHost, &oIP, &oAway);
        }
        IRCTADUser_FreeList(userList, numberUsers);
    }
    IRC_MFree(8, &command, &msgtarget, &msg, &oName, &oReal, &oHost, &oIP, &oAway);

    return IRC_OK;
}

/**
 * @brief commando Message of the Day
 * permite al usuario consultar el mensaje del día
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandMotd(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO MOTD 18
{
    int ret;
    char* command, *prefix, *target, *nick, *msg;

    command = prefix = target = nick = msg = NULL;

    prefix = SERVERNAME;
    getNickFromSocket(socket, &nick);
    //MOTDSTART
    if ((ret = IRCMsg_RplMotdStart(&command, prefix, nick, prefix)) != IRC_OK)
        return logIntError(ret, "error @ commandMotd -> IRCMsg_RplMotdStart\n");
    if (send(socket, command, strlen(command), 0) == -1)
        return logIntError(-1, "error @ commandMotd -> send\n");
    //MOTD
    free(command);
    if ((ret = IRCMsg_RplMotd(&command, prefix, nick, "Mambru se fue a la guerra. No se cuando vendra")) != IRC_OK)
        return logIntError(ret, "error @ commandMotd -> IRCMsg_RplMotd\n");
    if (send(socket, command, strlen(command), 0) == -1)
        return logIntError(-1, "error @ commandMotd -> send\n");
    //MOTDEND
    free(command);
    if ((ret = IRCMsg_RplEndOfMotd(&command, prefix, nick)) != IRC_OK)
        return logIntError(ret, "error @ commandMotd -> IRCMsg_RplEndOfMotd\n");
    if (send(socket, command, strlen(command), 0) == -1)
        return logIntError(-1, "error @ commandMotd -> send\n");


    free(command);
    if ((ret = IRCMsg_Motd(&command, prefix, target)) != IRC_OK)
        return logIntError(ret, "error @ commandMotd -> IRCMsg_Motd");
    if (send(socket, command, strlen(command), 0) == -1)
        return logIntError(-1, "error @ commandMotd -> send\n");
    IRC_MFree(5, &command, &target, &nick, &msg);

    return IRC_OK;
}

/**
 * @brief commando Whois
 * permite al usuario obtener información de cierto usuario en el servidor
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandWhois(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO WHOIS 31
{
    char *command, *hostname;
    char *prefix, *target, *maskarray, *nick;
    char *otherNick, *otherName, *otherHost, *otherReal, *otherIP, *otherAway, *channelsList = NULL, *answerList = NULL;
    long int otherID, creationTs, actionTs, numberOfChannels, retVal;
    int otherSocket;

    //ponemos todos estos campos a NULL y a 0 para las futuras llamadas a GetData
    prefix = target = maskarray = command = otherNick = otherName = otherHost = otherReal = otherIP = otherAway = NULL;
    otherID = creationTs = actionTs = otherSocket = 0;
    //nuestro hostname es SERVERNAME
    hostname = SERVERNAME;
    //obtenemos el nick del usuario que ha realizado este comando
    if ((retVal = getNickFromSocket(socket, &nick)) != IRC_OK)
        return logIntError(retVal, "error @ commandWhois -> getNickFromSocket");
    //parseamos el comando whois
    if ((retVal = IRCParse_Whois(strin, &prefix, &target, &otherNick)) != IRC_OK) {
        if (retVal == IRCERR_ERRONEUSCOMMAND) {
            if (IRCMsg_ErrNoNickNameGiven(&command, hostname, nick) != IRC_OK) {
                IRC_MFree(1, &nick);
                return logIntError(-1, "error @ commandWhois -> IRCMsg_ErrNoNickNameGiven");
            }
            if (send(socket, command, strlen(command), 0) == -1) {
                IRC_MFree(2, &nick, &command);
                return logIntError(-1, "error @ commandWhoIs -> send\n");
            }
            IRC_MFree(2, &nick, &command);
            return IRC_OK;
        }
        IRC_MFree(1, &nick);
        return logIntError(retVal, "error @ commandWhois -> IRCParse_Whois");
    }
    //obtenemos los datos del usuario por el cual se pregunta en el comando
    if ((retVal = IRCTADUser_GetData(&otherID, &otherName, &otherNick, &otherReal, &otherHost, &otherIP, &otherSocket, &creationTs, &actionTs, &otherAway)) != IRC_OK) {
        IRC_MFree(4, &nick, &prefix, &target, &otherNick);
        return logIntError(retVal, "error @ commandWhois -> IRCTADUser_GetData");
    }
    if (otherAway != NULL) {
        if ((retVal = IRCMsg_RplAway(&command, hostname, nick, otherNick, otherAway)) != IRC_OK) {
            IRC_MFree(9, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
            return logIntError(-1, "error @ commandPrivmsg -> IRCMsg_RplAway");
        }
        if (send(socket, command, strlen(command), 0) == -1) {
            IRC_MFree(10, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &command);
            return logIntError(-1, "error @ commandPrivmsg -> send (3)");
        }
        IRC_MFree(10, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &command);
        return IRC_OK;
    }

    //############################################ REPLY 311 WHOISUSER ############################################
    //preparamos el mensaje de respuesta 311 a whoisUser
    if ((retVal = IRCMsg_RplWhoIsUser(&command, hostname, nick, otherNick, otherName, otherHost, otherReal)) != IRC_OK) {
        IRC_MFree(9, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
        return logIntError(retVal, "error @ commandWhois -> IRCMsg_RplWhoIsUser");
    }
    //enviamos el mensaje 311
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(10, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
        return logIntError(-1, "error @ commandWhoIs -> send\n");
    }
    //IRC_MFree(10, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
    free(command);
    command = NULL;

    //############################################ REPLY 312 WHOISSERVER ############################################
    //preparamos el mensaje de respuesta 312 a whoisServer
    if ((retVal = IRCMsg_RplWhoIsServer(&command, hostname, nick, otherNick, otherHost, SERVERINFO)) != IRC_OK) {
        IRC_MFree(9, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
        return logIntError(retVal, "error @ commandWhois -> RplWhoIsServer");
    }
    //enviamos el mensaje 312
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(10, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
        return logIntError(-1, "error @ commandWhoIs -> send\n");
    }
    free(command);
    command = NULL;

    //############################################ REPLY 319 WHOISCHANNELS ############################################
    //obtenemos la lista de canales a los que pertenece el usuario por el que se pregunta
    if ((retVal = IRCTAD_ListChannelsOfUser(otherName, otherNick, &channelsList, &numberOfChannels)) != IRC_OK) {
        IRC_MFree(9, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
        return logIntError(retVal, "error @ commandWhois -> IRCTAD_ListChannelsOfUser");
    }
    /* no hago llamada a esta funcion porque eloy ya ha arreglado lo de las @ antes de los nombres de los canales
    if (parseChannelList(socket, channelsList, numberOfChannels, &answerList) != IRC_OK) {
        IRC_MFree(9, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway);
        return logIntError(retVal, "error @ commandWhois -> parseChannelList");
    }*/
    //preparamos el mensaje 319 whoisChannels
    if ((retVal = IRCMsg_RplWhoIsChannels(&command, hostname, nick, otherNick, channelsList)) != IRC_OK) {
        IRC_MFree(11, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);
        return logIntError(retVal, "error @ commandWhois -> IRCMsg_RplWhoIsChannels");
    }
    //enviamos el mensaje 319
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(12, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);
        return logIntError(-1, "error @ commandWhoIs -> send\n");
    }
    free(command);
    command = NULL;

    //############################################ REPLY 317 WHOISIDLE ############################################
    //preparamos el mensaje 317 whoisidle
    //TODO sustituir el 1 del parametro 5 por el numero de segundos que el usuario de nick otherNick haya estado idle
    if ((retVal = IRCMsg_RplWhoIsIdle(&command, hostname, nick, otherNick, 1, "segundos sin interactuar")) != IRC_OK) {
        IRC_MFree(11, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);
        return logIntError(retVal, "error @ commandWhois -> IRCMsg_RplWhoIsIdle");
    }
    //enviamos el mensaje 317
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(12, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);
        return logIntError(-1, "error @ commandWhoIs -> send\n");
    }
    free(command);
    command = NULL;

    //############################################ REPLY 318 ENDWHOIS ############################################
    //preparamos el mensaje 318 que marca el final de las respuestas del comando whois
    if ((retVal = IRCMsg_RplEndOfWhoIs(&command, hostname, nick, otherNick)) != IRC_OK) {
        IRC_MFree(11, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);
        return logIntError(retVal, "error @ commandWhois -> IRCMsg_RplEndOfWhoIs");
    }
    //enviamos el mensaje 318
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(12, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);
        return logIntError(-1, "error @ commandWhoIs -> send\n");
    }
    IRC_MFree(12, &command, &nick, &prefix, &target, &otherNick, &otherName, &otherReal, &otherHost, &otherIP, &otherAway, &channelsList, &answerList);

    return IRC_OK;
}

/**
 * @brief commando Ping
 * permite al usuario comprobar que la conexion con el servidor sigue estando activa
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandPing(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO PING 34
{
    char *prefix, *serverName, *serverName2, *msg, *command;
    long int retVal;
    prefix = serverName = serverName2 = msg = command = NULL;
    if ((retVal = IRCParse_Ping(strin, &prefix, &serverName, &serverName2, &msg)) != IRC_OK)
        return logIntError(retVal, "error @ commandPing -> IRCParse_Ping\n");

    free(prefix);
    prefix = SERVERNAME;
    //getHostName(server, &prefix);
    if ((retVal = IRCMsg_Pong(&command, prefix, prefix, serverName2, serverName)) != IRC_OK) {
        IRC_MFree(3, &serverName, &serverName2, &msg);
        return logIntError(retVal, "error @ commandPing -> IRCMsg_Pong\n");
    }
    if (send(socket, command, strlen(command), 0) == -1) {
        IRC_MFree(4, &command, &serverName, &serverName2, &msg);
        return logIntError(-1, "error @ commandPing -> send\n");
    }
    IRC_MFree(4, &command, &serverName, &serverName2, &msg);
    return IRC_OK;
}

/**
 * @brief commando Away
 * permite al usuario marcarse como ausente
 *
 * @param socket el socket del usuario a ver
 * @param client los datos del dispositivo cliente
 * @param client los datos del dispositivo servidor
 * @param strin el commando recibido
 *
 * @return IRC_OK si fue bien, otra cosa si no
 */
long int commandAway(int socket, struct sockaddr_in *client, struct sockaddr_in *server, char* strin)//TODO AWAY 37
{
    int id = 0;
    long ret = 0;
    char *prefix, *msg, *command, *user, *nick, *real;
    prefix = msg = command = user = nick = real = NULL;
    if ((ret = IRCParse_Away(strin, &prefix, &msg)) != IRC_OK)
        return logIntError(ret, "error @ commandAway -> IRCParse_Away\n");

    free(prefix);
    prefix = SERVERNAME;
    getNickFromSocket(socket, &nick);
    if ((ret = IRCTADUser_SetAway(id, user, nick, real, msg)) != IRC_OK) {
        free(nick);
        return logIntError(ret, "error @ commandAway -> IRCTADUser_SetAway\n");
    }

    if (msg == NULL) {
        if ((ret = IRCMsg_RplUnaway(&command, prefix, nick)) != IRC_OK) {
            IRC_MFree(2, &msg, &nick);
            return logIntError(ret, "error @ commandAway -> IRCMsg_RplUnaway\n");
        }
    } else {
        if ((ret = IRCMsg_RplNowAway(&command, prefix, nick)) != IRC_OK) {
            free(nick);
            return logIntError(ret, "error @ commandAway -> IRCMsg_RplNowAway\n");
        }
    }
    if ((ret = send(socket, command, strlen(command), 0)) == -1) {
        IRC_MFree(3, &msg, &nick, &command);
        return logIntError(ret, "error @ commandAway -> send\n");
    }
    free(command);
    command = NULL;
    if (msg != NULL) {
        if ((ret = IRCMsg_RplAway(&command, prefix, nick, nick, msg)) != IRC_OK) {
            IRC_MFree(3, &msg, &nick, &command);
            return logIntError(ret, "error @ commandAway -> IRCMsg_RplAway\n");
        }
    } else {
        if ((ret = IRCMsg_RplUnaway(&command, prefix, nick)) != IRC_OK) {
            IRC_MFree(3, &msg, &nick, &command);
            return logIntError(ret, "error @ commandAway -> IRCMsg_RplAway\n");
        }
    }
    if ((ret = send(socket, command, strlen(command), 0)) == -1) {
        IRC_MFree(3, &msg, &nick, &command);
        return logIntError(ret, "error @ commandAway -> send\n");
    }
    //TODO free
    IRC_MFree(3, &msg, &nick, &command);
    return IRC_OK;
}
