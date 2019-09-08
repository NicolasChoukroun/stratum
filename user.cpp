
#include "stratum.h"

#include "md5.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

// sql injection security, unwanted chars
void db_check_user_input(char* input)
{
    char *p = NULL;
    if (input && input[0]) {
        p = strpbrk(input, " \"'\\");
        if(p) *p = '\0';
    }
}

int db_get_coinid(YAAMP_DB *db, char* symbol)
{

    size_t len = strlen(symbol);
    if (len >= 2 && len <= 12) {
#ifdef NO_EXCHANGE
        db_query(db, "SELECT id FROM coins WHERE installed AND symbol='%s'",  symbol);
#else
        db_query(db, "SELECT id FROM coins WHERE installed AND (symbol='%s' OR symbol2='%s')", symbol, symbol);
#endif
        MYSQL_RES *result = mysql_store_result(&db->mysql);
        *symbol = '\0';
        if (!result) return 0;
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            //printf("fuckx 0= %s 1= %s 2= %s 3= %s \n",row[0],row[1],row[2],row[3]);
            return atoi(row[0]);
        }
        mysql_free_result(result);
    } else {
        return 0;
    }
}


void db_check_symbol(YAAMP_DB *db, char* symbol)
{
    if (!symbol) return;
    size_t len = strlen(symbol);


    if (len >= 2 && len <= 12) {
#ifdef NO_EXCHANGE
        db_query(db, "SELECT symbol FROM coins WHERE installed AND algo='%s' AND symbol='%s'", g_stratum_algo, symbol);
#else
        db_query(db, "SELECT symbol FROM coins WHERE installed AND (symbol='%s' OR symbol2='%s')", symbol, symbol);
#endif
        MYSQL_RES *result = mysql_store_result(&db->mysql);
        *symbol = '\0';
        if (!result) return;
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            strcpy(symbol, row[0]);
        }
        mysql_free_result(result);
    } else {
        *symbol = '\0';
    }
}

void db_add_user(YAAMP_DB *db, YAAMP_CLIENT *client)
{

    db_clean_string(db, client->username);
    db_clean_string(db, client->password);
    db_clean_string(db, client->version);
    db_clean_string(db, client->notify_id);
    db_clean_string(db, client->worker);

    if (strlen(client->username)==0) return;

    char symbol[16] = { 0 };
    char pass[126] = { 0 };

    char *p = strstr(client->password, "c=");
    if(!p) p = strstr(client->password, "s=");
    if(p) {
        strncpy(symbol, p + 2, 15);
        strncpy(client->symbol, p + 2, 15);
        db_clean_string(db, client->symbol);
    }
    p = strchr(symbol, ',');
    if(p) *p = '\0';

    char *p1= strstr(client->password, "p=");
    if (p1) {
        strncpy(pass, p1 + 2, 125);
        p1 = strchr(pass, ',');
        if (p1) *p1 = '\0';
    }

    if (strlen(symbol)<2) {
        strcpy(symbol, (const char *)"KYF\0");
        strcpy(client->symbol, (const char *)"KYF\0" );
    }
    if (strlen(pass)<5) strncpy(pass, (char *) client->password, 125);
    //printf("Symbol = %s - ",client->symbol);
    //printf("password = %s - ",pass);
    client->coinid = db_get_coinid(db, client->symbol);
    //printf("coinid = %d \n",client->coinid);


    char buffer3[1024];
    snprintf(buffer3, 1000,"Currently mining %s with id %d and address %s ", symbol, client->coinid, client->username);
    client_send_message(client, buffer3);

    strncpy(client->password,pass,64);

    bool guest = false;
    int gift = -1;
#ifdef ALLOW_CUSTOM_DONATIONS
    // donation percent
	p = strstr(client->password, "g=");
	if(p) gift = atoi(p+2);
	if(gift > 100) gift = 100;
#endif

    if (strlen(client->username)<=5 ) {
        debuglog("Username not good= %i \n", client->username);
        client_send_message(client, "username is wrong");
        client->userid=-1;
        return;
    }
    if (strlen(pass)<=5 ) {
        debuglog("password not good= %i \n", client->password);
        client_send_message(client, "password is wrong");
        client->userid=-1;
        return;
    }
    if (client->coinid==0 ) {
        client->userid=-1;
        client_send_message(client, "coin selection is wrong");
        debuglog("Coinid not good= %i \n", client->coinid);
        return;
    }

    if (strstr(client->version, "ncpool")==0 ) {
        // add pass
        debuglog("Wrong mining software, this mining pool is using a specific software '%s'\n", client->version);
        client_send_message(client, "mining software is wrong");
        client->userid=-3;
        return;
    }


    db_check_user_input(client->username);
    db_check_symbol(db, symbol);
    if(strlen(client->username) < MIN_ADDRESS_LEN) {
        if (!strlen(client->worker)) strncpy(client->worker, client->username,64);
        // allow benchmark / test / donate usernames
        /*if (!strcmp(client->username, "benchmark") || !strcmp(client->username, "donate") || !strcmp(client->username, "test")) {
            guest = true;
            if (g_list_coind.first) {
                CLI li = g_list_coind.first;
                YAAMP_COIND *coind = (YAAMP_COIND *)li->data;
                if (!strlen(client->worker)) strcpy(client->worker, client->username);
                strcpy(client->username, coind->wallet);
                if (!strcmp(client->username, "benchmark")) strcat(client->password, ",stats");
                if (!strcmp(client->username, "donate")) gift = 100;
            }
        }
        if (!guest) {*/
        //client_send_message(client, "username is wrong 2");
        debuglog("Invalid user address '%s'\n", client->username);
        client->userid=-4;
        return;
        //}
    }

    //debuglog("User %s symbol %s \n", client->username, client->symbol);
    db_query(db, "SELECT id, is_locked, logtraffic, coinid, donation,pass FROM accounts WHERE username='%s' and coinid=%i", client->username,client->coinid);

    //db_query(db, "SELECT id, is_locked, logtraffic, coinid, donation,pass FROM accounts WHERE username='%s'", client->username);

    MYSQL_RES *result = mysql_store_result(&db->mysql);
    if(result) {

        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            // locked
            if (row[1] && atoi(row[1])) {
                client->userid = -1;
                clientlog(client, "username is wrong 3");
                client_send_message(client, "username is wrong 3");
                mysql_free_result(result);
                return;
            }
            else client->userid = atoi(row[0]);

            client->logtraffic = row[2] && atoi(row[2]);
            client->coinid = row[3] ? atoi(row[3]) : 0;
            if (gift == -1) gift = row[4] ? atoi(row[4]) : 0; // keep current
            // wrong pass
            if (strcmp(pass ,row[5])!=0) {
                clientlog(client, "password is wrong 3");
                //db_check_user_input(row[5]);
                //strncpy(client->password,  row[5], 64);
                // add pass
                client->userid = -3;
                mysql_free_result(result);
                return;
            }
        }
    }
    mysql_free_result(result);

    // check double worker
    //debuglog("user %s %s \n", client->username, symbol, gift);


    if (gift < 0) gift = 0;
    client->donation = gift;


    if (strlen(pass) <= MIN_PASSWORD) {
        client_send_message(client, "password is wrong 2");
        debuglog("Password too short: %s\n",pass);
        client->userid = -3;
        return;
    }


    if(client->userid == -1) {
        client_send_message(client, "userid is wrong");
        return;
    }


    else if(client->userid == 0 && strlen(client->username) >= MIN_ADDRESS_LEN )
    {
        // create account
        db_query(db, "INSERT INTO accounts ("
                     "username, coinsymbol, is_locked,  coinid, balance, donation, hostaddr,pass) "
                     "values ('%s','%s', 1, ,'%d', 0, %d, '%s', '%s')",
                 client->username, client->symbol, client->coinid, gift, client->sock->ip,pass);

        client->userid = (int)mysql_insert_id(&db->mysql);
    }

    else {
        db_query(db, "UPDATE accounts SET coinsymbol='%s', swap_time=%u, donation=%d, hostaddr='%s' WHERE id=%d AND balance = 0"
                     " AND (SELECT COUNT(id) FROM payouts WHERE account_id=%d AND tx IS NULL) = 0" // failed balance
                     " AND (SELECT pending FROM balanceuser WHERE userid=%d ORDER by time DESC LIMIT 1) = 0" // pending balance
                , client->symbol, (uint) time(NULL), gift, client->sock->ip, client->userid, client->userid, client->userid);
        if (mysql_affected_rows(&db->mysql) > 0 && strlen(client->symbol)) {
            //debuglog("%s: %s coinsymbol set to %s %s ip %s uid (%d)\n",g_current_algo->name, client->username, symbol, client->coinid, client->sock->ip, client->userid);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////

void db_clear_worker(YAAMP_DB *db, YAAMP_CLIENT *client)
{
    if(!client->workerid)
        return;

    db_query(db, "DELETE FROM workers WHERE id=%d", client->workerid);
    client->workerid = 0;
}

void db_add_worker(YAAMP_DB *db, YAAMP_CLIENT *client)
{
    //if (db->mysql) return;

    char password[128] = { 0 };
    char version[128] = { 0 };
    char worker[128] = { 0 };

    int now = time(NULL);

    db_clear_worker(db, client);

    db_check_user_input(client->username);
    db_check_user_input(client->version);
    db_check_user_input(client->password);
    db_check_user_input(client->worker);
    if (strlen(client->username)==0) {
        clientlog(client, "username empty");
        return;
    }

    // strip for recent mysql defaults (error if fields are too long)
    if (strlen(client->password) > 64 || strlen(client->password)<2)
        clientlog(client, "password too long truncated: %s", client->password);
    if (strlen(client->version) > 64)
        clientlog(client, "version too long truncated: %s", client->version);
    if (strlen(client->worker) > 64)
        clientlog(client, "worker too long truncated: %s", client->worker);
    if (strlen(client->worker) <2 )
        strncpy(client->worker,client->username,64);
    // check if worker is a user

    db_query(db, "SELECT id, is_locked, logtraffic, coinid, donation,pass FROM accounts WHERE username='%s' and pass='%s'",
             client->username,client->password);

    MYSQL_RES *result = mysql_store_result(&db->mysql);
    if(!result) {
        client_send_message(client, "worker deos not match mendatory credentials");
        clientlog(client, "worker does not match mendatory credentials %s\n", client->username);
        mysql_free_result(result);
        client->userid=-1;
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if(row)
    {

        if (strcmp(client->password, row[5])!=0 ) {
            client_send_message(client, "worker password is wrong");
            clientlog(client, "worker password is wrong: %s - %s\n", client->password,row[5]);
            mysql_free_result(result);
            client->userid=-2;
            return;
        }
    }

    if(row[1] && atoi(row[1])) {
        client_send_message(client, "worker is locked");
        clientlog(client, "worker is locked %s\n", client->username);
        client->userid = -1;return;
    }
    else client->userid = atoi(row[0]);


    mysql_free_result(result);

    //strncpy(password, client->password, 64);
    //strncpy(version, client->version, 64);
    //strncpy(worker, client->worker, 64);


    db_query(db, "SELECT id FROM workers WHERE worker='%s' and coinid='%d' and name='%s'",client->worker,client->coinid,client->username);
    result = mysql_store_result(&db->mysql);
    if (result) {
        if ( mysql_num_rows( result )==0) {

            db_query(db,"INSERT INTO workers (userid, coinid, ip, name, difficulty, version, password, worker, algo, time, pid) "\
            "VALUES (%d, %d, '%s', '%s', %f, '%s', '%s', '%s', '%s', %d, %d)",
                     client->userid, client->coinid, client->sock->ip, client->username, client->difficulty_actual,
                     client->version, client->password, client->worker, g_stratum_algo, now, getpid());
            client->workerid = (int) mysql_insert_id(&db->mysql);
            //client_send_message(client, "worker added");
            clientlog(client,"ADDWORKER: userid=%i - pass=%s - version= %s - workerid=%i - worker=%s\n",client->userid,client->password,client->version,client->workerid,client->worker);
        }
    }


}

void db_update_workers(YAAMP_DB *db)
{
    g_list_client.Enter();
    for(CLI li = g_list_client.first; li; li = li->next)
    {
        YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
        if(client->deleted) continue;
        if(!client->workerid) continue;

        if(client->speed < 0.00001)
        {
            clientlog(client, "speed %f", client->speed);
            shutdown(client->sock->sock, SHUT_RDWR);
            db_clear_worker(db, client);
            object_delete(client);
            continue;
        }

        client->speed *= 0.8;
        if(client->difficulty_written == client->difficulty_actual) continue;

        db_query(db, "UPDATE workers SET difficulty=%f,coinid=%d, subscribe=%d WHERE id=%d",
                 client->difficulty_actual,client->coinid, client->extranonce_subscribe, client->workerid);
        client->difficulty_written = client->difficulty_actual;
    }

    client_sort();
    g_list_client.Leave();
}

void db_init_user_coinid(YAAMP_DB *db, YAAMP_CLIENT *client)
{
    if (!client->userid)
        return;


    db_query(db, "UPDATE accounts SET coinid=%d WHERE id=%d AND IFNULL(coinid,0) = 0",client->coinid, client->userid);
}
