#include <stdio.h>
#include <stdbool.h>
#include <bcm2835.h>
#include <time.h>
#include <sqlite3.h>
#include <microhttpd.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>

long long int simdikiZaman() {
    struct timespec simdi;
    clock_gettime(CLOCK_REALTIME, &simdi);
    return (long long int)(simdi.tv_sec);
}

char *zamanYazi(long long int zamansayi) {
    tzset();
    char *yazi = malloc(64);
    time_t zaman = (time_t)(zamansayi);
    struct tm* zamanyapi = localtime(&zaman);
    strftime(yazi, 64, "%d/%m/%Y %H:%M:%S", zamanyapi);
    return yazi;
}

sqlite3 *veritabani;
char *komut;
bool anlikhareket;
long long int baslangiczaman;

enum MHD_Result yanitla(void *cls, struct MHD_Connection *baglanti, const char *url, const char *metot, const char *versiyon, const char *veri, size_t *veriboyutu, void **concls){
    char *yanit;
    struct MHD_Response *yanityapi;
    int yanitkodu;
    cJSON *yanitobje = cJSON_CreateObject();
    if (strcmp(url, "/hareketDurumu") == 0 && strcmp(metot, "GET") == 0){
        sqlite3_stmt *kayitlar;
        char *komut = "SELECT * FROM HAREKET";
        if(sqlite3_prepare_v2(veritabani, komut, -1, &kayitlar, NULL)!=SQLITE_OK) {
            printf("Fehler beim Abfragen der Datensätze: %s\n", sqlite3_errmsg(veritabani));
            cJSON_AddNumberToObject(yanitobje, "yanitKodu", 500);
            cJSON_AddStringToObject(yanitobje, "hataMesaji", sqlite3_errmsg(veritabani));
            yanit = cJSON_Print(yanitobje);
            yanityapi = MHD_create_response_from_buffer(strlen(yanit), (void *)yanit, MHD_RESPMEM_MUST_FREE);
            yanitkodu = MHD_queue_response(baglanti, MHD_HTTP_INTERNAL_SERVER_ERROR, yanityapi);
        }
        else{
            cJSON_AddNumberToObject(yanitobje, "yanitKodu", 200);
            cJSON_AddBoolToObject(yanitobje, "anlikHareket", anlikhareket);
            cJSON_AddNumberToObject(yanitobje, "baslangicZaman", baslangiczaman);
            cJSON *kayitlardizi = cJSON_CreateArray();
            while (sqlite3_step(kayitlar) == SQLITE_ROW) {
                long long int id = sqlite3_column_int64(kayitlar, 0);
                long long int baslangic = sqlite3_column_int64(kayitlar, 1);
                long long int bitis = sqlite3_column_int64(kayitlar, 2);
                //printf("ID: %lld\t%s\t%s\n", id, zamanYazi(baslangic), zamanYazi(bitis));
                cJSON *kayit = cJSON_CreateObject();
                cJSON_AddNumberToObject(kayit, "baslangic", baslangic);
                cJSON_AddNumberToObject(kayit, "bitis", bitis);
                cJSON_AddItemToArray(kayitlardizi, kayit);
            }
            cJSON_AddItemToObject(yanitobje, "kayitlar", kayitlardizi);
            yanit = cJSON_Print(yanitobje);
            yanityapi = MHD_create_response_from_buffer(strlen(yanit), (void *)yanit, MHD_RESPMEM_MUST_FREE);
            yanitkodu = MHD_queue_response(baglanti, MHD_HTTP_OK, yanityapi);
        }
        sqlite3_finalize(kayitlar);
    }
    else{
        cJSON_AddNumberToObject(yanitobje, "yanitKodu", 404);
        yanit = cJSON_Print(yanitobje);
        yanityapi = MHD_create_response_from_buffer(strlen(yanit), (void *)yanit, MHD_RESPMEM_MUST_FREE);
        yanitkodu = MHD_queue_response(baglanti, MHD_HTTP_NOT_FOUND, yanityapi);
    }
    MHD_destroy_response(yanityapi);
    cJSON_Delete(yanitobje);
    return yanitkodu;
}

void onesignalPush(char *baslik, char *mesaj){
    CURL *curl;
    CURLcode yanit;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        cJSON *form = cJSON_CreateObject();
        cJSON_AddStringToObject(form, "app_id", "ONESIGNAL_APP_ID");
        cJSON *included_segments = cJSON_CreateArray();
        cJSON_AddItemToArray(included_segments, cJSON_CreateString("Subscribed Users"));
        cJSON_AddItemToObject(form, "included_segments", included_segments);
        cJSON *contents = cJSON_CreateObject();
        cJSON_AddItemToObject(contents, "en", cJSON_CreateString(mesaj));
        cJSON_AddItemToObject(form, "contents", contents);
        cJSON *headings = cJSON_CreateObject();
        cJSON_AddItemToObject(headings, "en", cJSON_CreateString(baslik));
        cJSON_AddItemToObject(form, "headings", headings);
        char *formyazi = cJSON_PrintUnformatted(form);

        struct curl_slist *headerlar = NULL;
        headerlar = curl_slist_append(headerlar, "Authorization: Basic ONESIGNAL_API_KEY");
        headerlar = curl_slist_append(headerlar, "Content-Type: application/json");
        headerlar = curl_slist_append(headerlar, "Accept: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "https://onesignal.com/api/v1/notifications");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formyazi);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlar);
        yanit = curl_easy_perform(curl);
        if (yanit != CURLE_OK){
            printf("POST-Anfrage fehlgeschlagen: %s\n", curl_easy_strerror(yanit));
        }
        else{
            printf("\n%s: %s\n", baslik, mesaj);
        }
        cJSON_Delete(form);
        curl_free(formyazi);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headerlar);
    }
    curl_global_cleanup();
}

int main(){
    if(sqlite3_open("hareketgecmisi.db", &veritabani)){
        printf("Datenbank konnte nicht geöffnet werden!\n");
        return 1;
    }
    komut = "CREATE TABLE IF NOT EXISTS HAREKET(ID INTEGER PRIMARY KEY AUTOINCREMENT, BASLANGIC INTEGER NOT NULL, BITIS INTEGER NOT NULL);";
    char *hata = NULL;
    if(sqlite3_exec(veritabani, komut, NULL, 0, &hata)!=SQLITE_OK){
        printf("Fehler beim Erstellen der Tabelle: %s\n", hata);
        sqlite3_free(hata);
        return 2;
    }
    anlikhareket = false;
    baslangiczaman = -1;

    struct MHD_Daemon *sunucu = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, 8080, NULL, NULL, &yanitla, NULL, MHD_OPTION_END);
    if(!sunucu){
        printf("Netzwerkserver konnte nicht gestartet werden!\n");
        return 3;
    }

    if (!bcm2835_init()){
        printf("Initialisierung der BCM2835-Bibliothek fehlgeschlagen!\n");
        return 4;
    }
    bcm2835_gpio_fsel(RPI_BPLUS_GPIO_J8_21, BCM2835_GPIO_FSEL_INPT);  // GPIO9 pinini input olarak ayarla
    bcm2835_gpio_set_pud(RPI_BPLUS_GPIO_J8_21, BCM2835_GPIO_PUD_UP);  // Dahili pull-up direncini aktif et
    
    long long int sonaktifzaman = -1;
    
    while(true){
        if (bcm2835_gpio_lev(RPI_BPLUS_GPIO_J8_21) == HIGH){
            sonaktifzaman = simdikiZaman();
            if(!anlikhareket){
		anlikhareket = true;
                baslangiczaman = simdikiZaman();
                printf("%s\tDie Bewegung hat begonnen!\n", zamanYazi(baslangiczaman));
                char *mesaj = calloc(64, sizeof(char));
                sprintf(mesaj, "%s - Die Bewegung hat begonnen!", zamanYazi(simdikiZaman()));
                onesignalPush("Bewegungsmelder", mesaj);
                free(mesaj);
            }
        }
        else if(simdikiZaman()-sonaktifzaman>=10 && anlikhareket){
            printf("\n%s\t-\t%s\n\n", zamanYazi(baslangiczaman), zamanYazi(sonaktifzaman));
            printf("%s\tDie Bewegung ist vorbei!\n", zamanYazi(simdikiZaman()));
            komut = calloc(128, sizeof(char));
            sprintf(komut, "INSERT INTO HAREKET(BASLANGIC, BITIS) VALUES (%lld, %lld);", baslangiczaman, sonaktifzaman);
            if(sqlite3_exec(veritabani, komut, NULL, 0, &hata)!=SQLITE_OK){
                printf("Fehler beim Speichern: %s\n", hata);
            }
            free(komut);
            
            anlikhareket = false;
            baslangiczaman = -1;
            sonaktifzaman = -1;
        }
        delay(100);
    }

    MHD_stop_daemon(sunucu);
    return 0;
}
