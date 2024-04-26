#include "isa-ldapserver.h"

/***
 * Vytvořá nový záznam.
 * @param char *cn 
 * @param char *uid 
 * @param char *mail
 * @return nově vytvořený záznam 
*/
attributes *create_attributes(char *cn, char *uid, char *mail)
{
    attributes *new_attributes = (attributes *) malloc(sizeof(attributes));
    if (new_attributes == NULL)
    {
        fprintf(stderr, "Error: Memory allocation error!\n");
        exit(1);
    }

    new_attributes->cn = strdup(cn);
    new_attributes->uid = strdup(uid);
    new_attributes->mail = strdup(mail);
    new_attributes->next = NULL;

    return new_attributes;
}

/***
 * Vloží nový záznám do listu
 * @param attributes **head - začátek listu
 * @param attributes *new_attributes - nový záznam
*/
void append_attributes(attributes **head, attributes *new_attributes)
{
    if (*head == NULL)
        *head = new_attributes;
    else
    {
        attributes *current = *head;
        while (current->next != NULL)
            current = current->next;
        current->next = new_attributes;
    }
}

/***
 * Uvolní list
 * @param attributes *head - počátek listu
*/
void free_attributes_list(attributes *head)
{
    while (head != NULL)
    {
        attributes *temp = head;
        head = head->next;
        free(temp->cn);
        free(temp->uid);
        free(temp->mail);
        free(temp);
    }
}

/***
 * Porovná vstupní hodnotu value s sestaveným regulárním výrazem.
 * @param int filter - typ filtru (sutbstring, equalMatch)
 * @param char *value - vstup z databáze
 * @param char *assertion_value - filtr
*/
int compare_with_filter(int filter, char *value, char *assertion_value)
{
	if (filter == 163)
	{
		return !strcmp(value, assertion_value);
	}
	else if (filter == 164)
	{
		char regex[256] = "^";
		regex_t regex_comp;
		size_t i = 2;
		// převod filtry do stavu, se kterým se dá pracovat
		while (i < strlen(assertion_value))
		{
			int subs_type = (uint8_t) assertion_value[i - 2];
			int length = assertion_value[i - 1];
			char *item = malloc(length + 1);
            strncpy(item, &assertion_value[i], length);
            item[length] = '\0';
            i += length + 2;

			// počátečný string a poté libovovný znak a jeho počet poté
			if (subs_type == 128)
			{
				strcat(regex, item);
				strcat(regex, ".*");
			}
			// libovovný znak a jeho počet na začátku, poté string a poté 
			// libovovný znak a jeho počet na konci
			else if (subs_type == 129)
			{ 
				// odstranění duplicitních .*
				if (strlen(regex) == 0 || regex[strlen(regex) - 1] != '*')
					strcat(regex, ".*"); 
				strcat(regex, item);
				strcat(regex, ".*");
			}
			// libovovný znak a jeho počet ze začátku a poté string na konci
			else if (subs_type == 130)
			{
				// odstranění duplicitních .*
				if (strlen(regex) == 0 || regex[strlen(regex) - 1] != '*')
					strcat(regex, ".*"); 
				strcat(regex, item);
			}
		}
		strcat(regex, "$");

		if (regcomp(&regex_comp, regex, REG_EXTENDED | REG_NOSUB) != 0) 
		{
			fprintf(stderr, "Error: Could not compile regex.\n");
			return 0;
		}
        int result = regexec(&regex_comp, value, 0, NULL, 0);
        regfree(&regex_comp);
        return !result;
	}
	return 0;
}

/***
 * Čte řádky v souboru, rozdělí řádek na 3 části a aplikuje filtry
 * a vloží do res_content což je primitivní list/sekvence. Pokud není nastaven filtr,
 * tak tam vloží všechny řádky (pokud není nastaven limit).
 * @param char *filename - název souboru
 * @param char *attribute_desc - např uid
 * @param char *assertion_value - např xlogin00
 * @param attributes **res_content - výsledná sekvence výsledků
 * @param int filter - type filteru (substring, equalMatch)
*/
void search_database(char *filename, char *attribute_desc, char *assertion_value, 
						attributes **res_content, int filter) 
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) 
	{
        fprintf(stderr, "Error: Can't open file!");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    database db;

    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) 
	{
        line[strcspn(line, "\n")] = '\0';
        int fields_read = sscanf(line, "%[^;];%[^;];%s", db.cn, db.uid, db.mail);
		if (fields_read == 3)
		{
			// filtry nejsou nastaveny
			if (strcmp(attribute_desc, "objectclass"))
			{
				int match = 0;
				if (!strcmp(attribute_desc, "cn")) 
				{
					int res_code = compare_with_filter(filter, db.cn, assertion_value);
					if (res_code) match = 1;
				}
				else if (!strcmp(attribute_desc, "uid"))
				{ 
					int res_code = compare_with_filter(filter, db.uid, assertion_value);
					if (res_code) match = 1;
				}
				else if (!strcmp(attribute_desc, "mail"))
				{
					int res_code = compare_with_filter(filter, db.mail, assertion_value);
					if (res_code) match = 1;
				}
				else 
				{
					fprintf(stderr, "Error: Invalid attribute description: %s!\n", attribute_desc);
					exit(1);
				}
				
				// nalezen stejný string
				if (match) 
				{	
					append_attributes(
						res_content, 
						create_attributes(db.cn, db.uid, db.mail));
					match = 0;
				}
			} 
			else
			{
				append_attributes(
					res_content, 
					create_attributes(db.cn, db.uid, db.mail));
			}
		}
		else
		{
			fprintf(stderr, "Error: Can't parse values in file: %s!\n", filename);
			exit(1);
		}
    }
	
    fclose(file);
}

/***
 * Zpracuje příchozí požadavek searchRequest. Uloží všechny potřebné informace do
 * proměnných a sestaví potřebné stringy jako atributy a filtry. Poté zavolá
 * process_filter, tato funkce předělá filtry do stavu pro odpověd (vymaže prázdné znaky).
 * Zavolá funkci search_database, která prohledá file a poté zavolá send_response.
 * @param char *buff - searchRequest zpráva
 * @param int socket - číslo soketu
 * @param char *file_name - název souboru pro čtení
*/
void get_message(char *buff, int socket, char *file_name)
{
	int base_object_length = buff[8];
	int size_limit = buff[17 + base_object_length];
	int filter_length = buff[25 + base_object_length];
	int filter = (u_int8_t) buff[24 + base_object_length];
	int skip_base_filter = base_object_length + filter_length;
	int attributes_length = buff[27 + skip_base_filter];
	char *base_object = malloc(base_object_length + 1);
	char *filter_content = malloc(filter_length + 1);
	char *attribute_description = malloc(attributes_length + 1);

	for (int i = 0; i < base_object_length; i++)
	{
		base_object[i] = buff[9 + i];
	}
	base_object[base_object_length] = '\0';

	for (int i = 0; i < filter_length; i++)
	{
		filter_content[i] = buff[26 + base_object_length + i];
	}
	filter_content[filter_length] = '\0';

	for (int i = 0; i < attributes_length; i++)
	{
		attribute_description[i] = buff[28 + skip_base_filter + i];
	}
	attribute_description[attributes_length] = '\0';

	// zpracovuje filtry pro práci později
	char *proccesed_filter = NULL;
	char *attribute_desc = NULL;
	char *assertion_value = NULL;

	attributes *res_content = NULL;

	int ret_code = 0;
	// pokud filter existuje, tak se předá
	if (filter != 135)
	{
		ret_code = process_filter(filter_content, &proccesed_filter, &attribute_desc, &assertion_value);
		if (ret_code != 1)
		{
			search_database(file_name, attribute_desc, assertion_value, &res_content, filter);
			if (res_content != NULL)
				send_response(res_content,  
							attribute_description, 
							attributes_length, 
							base_object, 
							proccesed_filter,
							socket,
							size_limit);
		}
	}
	else
	{
		search_database(file_name, filter_content, "", &res_content, filter);
		if (res_content != NULL)
			send_response(res_content,  
					attribute_description, 
					attributes_length, 
					base_object, 
					filter_content,
					socket,
					size_limit);
	}

	// uvolnění
	if (base_object != NULL) free(base_object);
	if (filter_content != NULL) free(filter_content);
	if (attribute_description != NULL) free(attribute_description);
	if (proccesed_filter != NULL) free(proccesed_filter);
	if (attribute_desc != NULL) free(attribute_desc);
	if (assertion_value != NULL) free(assertion_value);

	// chyba při malokaci
	if (ret_code == 1) exit(1);
}

/***
 * Zpracuje filtry a převede je do tvaru 'uid=xlogin00' pro odeslání bindResponse
 * @param char *filter - zprává filtru s bílími znaky
 * @param char **result - výsledná zpráva
 * @param char **attribute_desc - např uid
 * @param char **assertion_value - např xlogin00
 * @return error code
*/
int process_filter(char *filter, char **result, char **attribute_desc, char **assertion_value) 
{	
	int attribute_desc_length = filter[1];
	int assertion_value_length = filter[3 + attribute_desc_length];

	*attribute_desc = (char *) malloc(attribute_desc_length + 1);
	*assertion_value =(char *) malloc(assertion_value_length + 1);

	if (*attribute_desc == NULL) 
	{
		fprintf(stderr, "Error: Can't malloc 'attribute_desc'!\n");
		return 1;	
	}
	if (*assertion_value == NULL) 
	{
		fprintf(stderr, "Error: Can't malloc 'assertion_value'!\n");
		return 1;
	}

	for (int i = 0; i < attribute_desc_length; i++)
		(*attribute_desc)[i] = filter[2 + i];
	(*attribute_desc)[attribute_desc_length] = '\0';

	for (int i = 0; i < assertion_value_length; i++)
		(*assertion_value)[i] = filter[4 + attribute_desc_length + i];
	(*assertion_value)[assertion_value_length] = '\0';

    int len = attribute_desc_length + assertion_value_length + 2;

    *result = (char *) malloc(len);

	if (*result == NULL) 
	{
		fprintf(stderr, "Error, can't malloc 'result'!\n");
		return 1;
	}
	
    for (int i = 0; i < attribute_desc_length; i++) 
        (*result)[i] = filter[2 + i];

    (*result)[attribute_desc_length] = '=';

    for (int i = attribute_desc_length + 1; i < len; i++) 
        (*result)[i] = filter[3 + i];
    (*result)[len] = '\0';

	return 0;
}

/***
 * Funkce na spojení dvou stringu. 
 * @param char *array1 - první string 
 * @param char *array2 - druhý string
 * @param char **result - výsledek spojení
*/
void concat_hex_arrays(char *array1, char *array2, char **result) 
{
    size_t resultSize = strlen(array1) + strlen(array2) + 1; 
    *result = (char *) malloc(resultSize);

    if (*result == NULL) 
	{
        fprintf(stderr, "Memory allocation failed!\n");
        exit(1);
    }

    strcpy(*result, array1);
    strcat(*result, array2);
}

/***
 * Přidá číslo na začátek stringu,
 * @param int number - číslo
 * @param char *array - string
 * @param char **result - výsledný string 
 * @return error code
*/
int int_to_string(int number, char *array, char **result) 
{
    size_t resultSize = 2 + strlen(array); 
	char *copy = strdup(*result);
	free(*result);
	*result = (char *) malloc(resultSize);

	if (*result == NULL) 
	{
		fprintf(stderr, "Error: Memory allocation failed!\n");
		exit(1);
	}

    char asciiChar = (uint8_t)number;

	sprintf(*result, "%c%s", asciiChar, copy);
    return 0;
}

/**
 * @brief Odešle odpověd klientovi. Funkce buď
 * prochází do nekonečna a nebo tolikrát, kolik mu to umožní limit.
 * Začne se zpracováním atributu. Ve whilu, podle toho, jaké atributy jsou,
 * sestaví část stringu a uloží do result. Následně sestaví zbytek stirngu.
 * Používá primárně dvě funkce: int_to_string pro spojení čísla a stringu a
 * concat_hex_arrays pro spojení dvou stringu.
 * 
 * @param head - počátek listu
 * @param attribute_description - část zprávy searchRequest, oddíl atributy
 * @param attributes_length - délka části atributů
 * @param base_object - část zprávy zvaná base_object
 * @param proccesed_filter - filtry, ale už ve zpracovaném stavu pro práci
 * @param socket - číslo soketu
 * @param size_limit - počet zpráv, který se má generovat (-z)
 * @return error code 
 */
int send_response(attributes *head, char *attribute_description, 
					int attributes_length, char *base_object, 
					char *proccesed_filter, int socket, int size_limit)
{
	int limit = size_limit;
	int no_limit = 0;
	if (limit == 0) 
	{
		no_limit = 1;
		limit++;
	}
	attributes *current = head;

    while (current != NULL && limit > 0)
    {
		limit--;
		if (no_limit) limit++;
		int i = 1;
		char *result = NULL;

		// počátek, který je na pevno daný pro sestavení atributu
		char all_attrb[] = { 0x04, 0x02, 0x63, 0x6e, 0x04, 0x04, 0x6d, 0x61, 0x69, 0x6c, 0x04, 0x03, 0x75, 0x69, 0x64 };
		int all_attrb_length = sizeof(all_attrb) / sizeof(all_attrb[0]);

		char *selected_attrb;
		int selected_length;

		if (!attributes_length) 
		{
			selected_attrb = all_attrb;
			selected_length = all_attrb_length;
		} 
		else 
		{
			selected_attrb = attribute_description;
			selected_length = attributes_length;
		}

		while (i < selected_length)
		{
			// vezmi délku, pomocí délky zjisti atribut a vytvoř zprávu
			int length = selected_attrb[i];
			char *attrbs = malloc(length + 1);
			if (attrbs == NULL) 
			{
                fprintf(stderr, "Error: Memory allocation failed!\n");
                exit(1);
            }
			for (int j = 0; j < length; j++)
			{
				attrbs[j] = selected_attrb[i + 1];
				i++;
			}
			attrbs[length] = '\0';
			i += 2;

			// jeden ze zadaných atributu byl cn
			if (!strcmp(attrbs, "cn"))
			{
				if (result == NULL)
					result = strdup(current->cn);
				else
				{
					char *copy = strdup(result);
					concat_hex_arrays(current->cn, copy, &result);
					free(copy);
				}
				int vals_length = strlen(current->cn);
				int_to_string(vals_length, result, &result);
				int_to_string(4, result, &result);
				int_to_string(vals_length + 2, result, &result);
				int_to_string(49, result, &result);
				char *copy = strdup(result);
				concat_hex_arrays("cn", copy, &result);
				int_to_string(2, result, &result);
				int_to_string(4, result, &result);
				int partial_attrb_list_length = vals_length + 8;
				int_to_string(partial_attrb_list_length, result, &result);
				int_to_string(48, result, &result);

			}

			// jeden ze zadaných atributu byl uid
			else if (!strcmp(attrbs, "uid"))
			{
				if (result == NULL)
					result = strdup(current->uid);
				else
				{
					char *copy = strdup(result);
					concat_hex_arrays(current->uid, copy, &result);
					free(copy);
				}
				int vals_length = strlen(current->uid);
				int_to_string(vals_length, result, &result);
				int_to_string(4, result, &result);
				int_to_string(vals_length + 2, result, &result);
				int_to_string(49, result, &result);
				char *copy = strdup(result);
				concat_hex_arrays("uid", copy, &result);
				int_to_string(3, result, &result);
				int_to_string(4, result, &result);
				int partial_attrb_list_length = vals_length + 9;
				int_to_string(partial_attrb_list_length, result, &result);
				int_to_string(48, result, &result);
			}

			// jeden ze zadaných atributu byl mail
			else if (!strcmp(attrbs, "mail"))
			{
				if (result == NULL)
					result = strdup(current->mail);
				else
				{
					char *copy = strdup(result);
					concat_hex_arrays(current->mail, copy, &result);
					free(copy);
				}
				int vals_length = strlen(current->uid);
				int_to_string(vals_length, result, &result);
				int_to_string(4, result, &result);
				int_to_string(vals_length + 2, result, &result);
				int_to_string(49, result, &result);
				char *copy = strdup(result);
				concat_hex_arrays("mail", copy, &result);
				int_to_string(4, result, &result);
				int_to_string(4, result, &result);
				int partial_attrb_list_length = vals_length + 10;
				int_to_string(partial_attrb_list_length, result, &result);
				int_to_string(48, result, &result);
			}
			free(attrbs);
		}

		int_to_string(strlen(result), result, &result);
		int_to_string(48, result, &result);
		char *object_name = NULL;
		concat_hex_arrays("dc=fit,", base_object, &object_name);
		// filtry nebyly zadány
		if (strcmp(proccesed_filter, "objectclass"))
		{
			concat_hex_arrays(",", object_name, &object_name);
			concat_hex_arrays(proccesed_filter, object_name, &object_name);
		}
		concat_hex_arrays(object_name, result, &result);
		int_to_string(strlen(object_name), result, &result);
		int_to_string(4, result, &result);
		int_to_string(strlen(result), result, &result);
		int_to_string(100, result, &result);
		int_to_string(2, result, &result);
		int_to_string(1, result, &result);
		int_to_string(2, result, &result);
		int_to_string(strlen(result), result, &result);
		int_to_string(48, result, &result);

		// odeslání sestavené zprávy
		int bytes_sent = send(socket, result, strlen(result), 0);
		if (bytes_sent == -1) 
		{
			fprintf(stderr, "Error: Can't send LDAP message!\n");
			if (result != NULL) free(result);
				result = NULL;
			exit(1);
		}
		
		if (result != NULL) free(result);
		result = NULL;
        current = current->next;
    }
	
	return 0;
}
// obstarání Ctrl + C
void sigint_handler() 
{
	printf("\n");
	printf("KILL\n");
    ctrl_c_pressed = 1;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    exit(0);
}

// obstarání Ctrl + C
void setup_signal_handlers() 
{
    struct sigaction sig_a;
    sig_a.sa_handler = sigint_handler;
    sigemptyset(&sig_a.sa_mask);
    sig_a.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sig_a, NULL) == -1) {
        perror("Error setting up SIGCHLD handler");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGINT, &sig_a, NULL) == -1) {
        perror("Error setting up SIGINT handler");
        exit(EXIT_FAILURE);
    }
}
/***
 * Nastavi základní tcp komunikaci pro poslouchání klientů.
 * Jakmile přijde bindRequest, odešle bindResponse jako odpověď.
 * Poté čeká na searchRequest, po kterém zavolá funkci 
 * get_message(buff, &search_res_entry, comm_socket, file)
 * @param int port - číslo portu
 * @param char *file - soubor pro čtení
*/
void tcp(int port, char *file) 
{
    int rc;
    int welcome_socket;
    struct sockaddr_in6 sa;
	struct sockaddr_in6 sa_client;
    char str[INET6_ADDRSTRLEN];
	pid_t pid;

    socklen_t sa_client_len = sizeof(sa_client);
	if ((welcome_socket = socket(PF_INET6, SOCK_STREAM, 0)) < 0)
	{
		perror("Error: could not create the socket!\n");
		exit(1);
	}

    printf("Socket successfully created by socket()\n");

    memset(&sa, 0, sizeof(sa));
	sa.sin6_family = AF_INET6;
	sa.sin6_addr = in6addr_any;
	sa.sin6_port = htons(port);	

	int on = 1;
	rc = setsockopt(welcome_socket, SOL_SOCKET,  SO_REUSEADDR,
					(char *)&on, sizeof(on));
	if (rc < 0)
	{
		perror("ERROR: setsockopt");
		close(welcome_socket);
		exit(-1);
	}

    if ((rc = bind(welcome_socket, (struct sockaddr*) &sa, sizeof(sa))) < 0)
	{
		perror("Error: bind!\n");
		exit(1);		
	}

    printf("Socket succesfully bound to the port using bind()\n");

	if ((listen(welcome_socket, 1)) < 0)
	{
		perror("Error: listen\n");
		exit(1);				
	}

    printf("Waiting for incoming connections on port %d (%d)...\n", port, sa.sin6_port);

	// odpovědi pro bindRequest a bindResDone pro ukončení komunikace
	char bind_response[] = {0x30, 0x0c, 0x02, 0x01, 0x01, 0x61, 0x07, 0x0a, 0x01, 0x00, 0x04, 0x00, 0x04, 0x00};
	char search_res_done[] = {0x30, 0x0c, 0x02, 0x01, 0x02, 0x65, 0x07, 0x0a, 0x01, 0x00, 0x04, 0x00, 0x04, 0x00};
	setup_signal_handlers(); 
	while (!ctrl_c_pressed)
	{
		int comm_socket = accept(welcome_socket, (struct sockaddr*) &sa_client, &sa_client_len);		
		if (comm_socket > 0)
		{
			if ((pid = fork()) > 0) // proces rodiče
			{ 
				printf("parent: closing newsock and continue to listen to new incoming connections\n");
				close(comm_socket);
			}
			else if (pid == 0) // proces dítěte
			{ 
				close(welcome_socket);
				
				if(inet_ntop(AF_INET6, &sa_client.sin6_addr, str, sizeof(str))) 
				{
					printf("INFO: New connection:\n");
					printf("INFO: Client address is %s\n", str);
					printf("INFO: Client port is %d\n", ntohs(sa_client.sin6_port));
					printf("INFO: Client ws is %d\n", welcome_socket);
				}

				// nastav vypraný socket aby byl neblokující
				int flags = fcntl(comm_socket, F_GETFL, 0);
				rc = fcntl(comm_socket, F_SETFL, flags | O_NONBLOCK);
				if (rc < 0)
				{
					perror("ERROR: fcntl");
					exit(EXIT_FAILURE);
				}

				char buff[2048];
				int res = 0;
				int ldap_msg = 0;

				for (;;)		
				{	
					bzero(buff, 2048);
					res = recv(comm_socket, buff, 2048, MSG_DONTWAIT);

					 if (res > 0)
					{
						if (ldap_msg == 0) // odpověď bindResponse
						{
							send(comm_socket, bind_response, 14, 0);
							ldap_msg = 1;
						}
						else if (ldap_msg == 1) // odpověď searchResEntry
						{
							get_message(buff, comm_socket, file);
							send(comm_socket, search_res_done, 14, 0);
							ldap_msg = 2;
						} 
						else if (ldap_msg == 2) // odpověď searchResEntry
						{
							printf("Connection to %s closed\n", str);
							close(comm_socket);
							break;
						}
					}
					 else if (res == 0)
					{
						printf("INFO: %s closed connection.\n", str);
						close(comm_socket);
						break;
					}	
					else if (errno == EAGAIN || errno == EWOULDBLOCK)
						continue;
					else
					{
						perror("ERROR: recv");
						exit(EXIT_FAILURE);
					}
				}
			}
			else
			{
				fprintf(stderr,"Error: Fork failed!\n");
				exit(1);
			}
		}
	}
}

/***
 * Zpracuje argumenty jako port a file pomoci getopts a pokud port
 * neni zadan tak vychozi je 389
 * Pote zavola funkci tcp(port, file)
 * @param int argc - pocet argumentu
 * @param char *argv[] - argumenty
 */
int main(int argc, char *argv[])
{
    int opt;
    int port;
    char *file = NULL;

    while ((opt = getopt(argc, argv, "p:f:")) != -1) 
    {
        switch (opt)
        {
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                file = optarg;
                break;
            default:
                fprintf(stderr, "Usage: ./isa-ldapserver -p <port> -f <file>\n");
                exit(1);
        }
    }

    if (file == NULL) 
    {
        fprintf(stderr, "Usage: ./isa-ldapserver -p <port> -f <file>\n");
        exit(1);
    }
	if (access(file, 0) == -1) 
	{
        fprintf(stderr, "Error: File '%s' does not exist.\n", file);
		exit(1);
    }
	if (port == -1) port = 389;

    tcp(port, file);

	return 0;
}