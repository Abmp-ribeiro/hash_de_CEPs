#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#define SEED    0x12345678

/* ESTRUTURA DA TABELA */

typedef struct {
     uintptr_t * table;
     int size;
     int max;
     uintptr_t deleted;
     float taxaocup; // taxa de ocupacao da tabela
     char * (*get_key)(void *);
}thash;

/* ESTRUTURA DOS CEPS */

typedef struct {
    char cep_ini[6];
    char cep_fim[6];
    char cidade[50];
    char estado[3];
} tcep;

/* DECLARACOES DAS FUNCOES DA TABELA HASH*/

int hash_insere(thash * h, void * bucket);
void hash_duplicar(thash * h);
int hash_constroi(thash * h, int nbuckets, char * (*get_key)(void *), float taxaocup);
void *hash_busca(thash h, const char * key);
int hash_remove(thash * h, const char * key);
void hash_apaga(thash *h);

/* FUNCOES TABELA HASH */

uint32_t hashf(const char* str, uint32_t h){
    /* One-byte-at-a-time Murmur hash 
    Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp */
    for (; *str; ++str) {
        h ^= *str;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

uint32_t hashf2(const char* str) { // Aplicacao de uma segunda funcao hash $
    /* Bernstein hash
    Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp */
    uint32_t seed = 5381;
    
    for (; *str; ++str){
        seed = 33 * seed + (unsigned char)*str;
    }
    
    // Garantir que nunca retorna 0 para evitar step = 1 sempre
    return seed;
}

int hash_insere(thash *h, void *bucket) { // Insere elemento na tabela $
    // Garante que ha espaço antes de inserir
    while ((float)(h->size + 1) / h->max >= h->taxaocup)
        hash_duplicar(h);

    const char *key = h->get_key(bucket);
    uint32_t hash1 = hashf(key, SEED);
    uint32_t hash2 = hashf2(key);

    int pos = hash1 % h->max;
    int step = 1 + (hash2 % (h->max - 1));

    // cobre multipas duplicacoes
    int tentativas = 0;
    while (h->table[pos] && h->table[pos] != h->deleted && tentativas < h->max) {
        pos = (pos + step) % h->max;
        tentativas++;
    }
    // Se a tabela estiver cheia, duplicamos
    if (tentativas >= h->max) {
        hash_duplicar(h);
        return hash_insere(h, bucket);
    }

    h->table[pos] = (uintptr_t)bucket;
    h->size++;
    return EXIT_SUCCESS;
}



void hash_duplicar(thash * h){ // Duplica o tamanho da tabela $
    uintptr_t * tabela_anterior = h->table;
    int maximo_anterior = h->max;
    h->max *= 2;
    h->table = calloc(h->max, sizeof *h->table);
    if (h->table == NULL){
        fprintf(stderr, "Erro ao duplicar a tabela hash\n");
        exit(EXIT_FAILURE);
    }
    h->size = 0; // reset size
    for (int i = 0; i < maximo_anterior; i++){
        if (tabela_anterior[i] != 0 && tabela_anterior[i] != h->deleted){
            hash_insere(h, (void *)tabela_anterior[i]);
        }
    }
    free(tabela_anterior);
}


int hash_constroi(thash * h, int nbuckets, char * (*get_key)(void *), float taxaocup){
    h->table =calloc(sizeof(void *),nbuckets+1);
    if (h->table == NULL){
        return EXIT_FAILURE;
    }
    h->max = nbuckets+1;
    h->size = 0;
    h->deleted = (uintptr_t)&(h->size);
    h->get_key = get_key;
    h->taxaocup = taxaocup;
    return EXIT_SUCCESS;

}


void * hash_busca(thash h, const char * key){ // Alteracao para garantir o step correto $
    int pos = hashf(key,SEED) % (h.max);
    uint32_t hash2 = hashf2(key);
    int step = 1 + (hash2 % (h.max - 1));
    
    // Garantir step válido
    if (step <= 0 || step >= h.max) {
        step = 1;
    }
    
    int tentativas = 0;
    while(h.table[pos] != 0 && tentativas < h.max){
        if (h.table[pos] != h.deleted && strcmp(h.get_key((void *)h.table[pos]),key) == 0){
            return (void *)h.table[pos]; 
        }
        pos = (pos + step) % h.max;
        tentativas++;
    }
    return NULL;
}

int hash_remove(thash * h, const char * key){ // Alterado para dar o step $
    int pos = hashf(key,SEED) % h->max;
    uint32_t hash2 = hashf2(key);
    int step = 1 + (hash2 % (h->max - 1));
    
    // Garantir step válido
    if (step <= 0 || step >= h->max) {
        step = 1;
    }
    
    int tentativas = 0;
    while(h->table[pos]!=0 && tentativas < h->max){
        if (h->table[pos] != h->deleted && strcmp(h->get_key((void *)h->table[pos]),key) == 0){ 
            free((void *)h->table[pos] );
            h->table[pos] = h->deleted;
            h->size -=1;
            return EXIT_SUCCESS; 
        }
        pos = (pos + step) % h->max;
        tentativas++;
    }
    return EXIT_FAILURE;
}

void hash_apaga(thash *h){
    int pos;
    for(pos =0;pos< h->max;pos++){
        if (h->table[pos] != 0){
            if (h->table[pos]!=h->deleted){
                free((void *)h->table[pos]);
            }
        }
    }
    free(h->table);
}



/* FUNCOES ESTRUTURA CEP */ 

char * get_key(void * reg){ 
    return ((tcep *)reg)->cep_ini;
}

void * aloca_cep(char * cep_ini, char *cep_fim, char * cidade, char * estado){
    tcep *_cep = (tcep *)malloc(sizeof(tcep));
    strcpy(_cep->cep_ini,cep_ini);
    strcpy(_cep->cep_fim,cep_fim);
    strcpy(_cep->cidade,cidade);
    strcpy(_cep->estado,estado);
    return _cep;
}

/* FUNCOES DATASET */

void ler_CSV(FILE *file, thash *h) {
    char line[256];
    fgets(line, sizeof(line), file);

    while (fgets(line, sizeof(line), file)) {
        char estado[3], cidade[50], cep_ini[7], cep_fim[7];
        memset(estado, 0, sizeof(estado));
        memset(cidade, 0, sizeof(cidade));
        memset(cep_ini, 0, sizeof(cep_ini));
        memset(cep_fim, 0, sizeof(cep_fim));

        line[strcspn(line, "\n")] = 0;

        char *token = strtok(line, ",");
        if (!token) continue;
        strncpy(estado, token, 2);
        estado[2] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(cidade, token, sizeof(cidade) - 1);
        cidade[sizeof(cidade) - 1] = '\0';

        token = strtok(NULL, ","); // Faixa de CEP (ignora)
        
        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(cep_ini, token, 5);
        cep_ini[5] = '\0';

        token = strtok(NULL, ",");
        if (!token) continue;
        strncpy(cep_fim, token, 5);
        cep_fim[5] = '\0';

        tcep *novo = (tcep *)aloca_cep(cep_ini, cep_fim, cidade, estado);
        
        int resultado;
        
        resultado = hash_insere(h, novo);
        
        if (resultado == EXIT_FAILURE) {
            printf("Erro ao inserir CEP %s\n", cep_ini);
            free(novo);
        }
    }
}

int constroi_dataset(thash * h, int nbuckets, char * (*get_key)(void *), float taxaocup){
    if (hash_constroi(h, nbuckets, get_key, taxaocup) == EXIT_FAILURE) {
        fprintf(stderr, "Erro ao construir a tabela hash\n");
        return EXIT_FAILURE;
    }   
    FILE *file = fopen("ceps.csv", "r");
    if (!file) {
        fprintf(stderr, "Erro ao abrir o arquivo cep.csv\n");
        return EXIT_FAILURE;
    }
    ler_CSV(file, h);
    fclose(file);

    return EXIT_SUCCESS;
}

/* DECLARACOES DAS FUNCOES DE TESTE DE BUSCA */ 

void teste_busca();
void busca_padrao(thash h);
int teste_busca10ocup(int nbuckets);
int teste_busca20ocup(int nbuckets);
int teste_busca30ocup(int nbuckets);
int teste_busca40ocup(int nbuckets);
int teste_busca50ocup(int nbuckets);
int teste_busca60ocup(int nbuckets);
int teste_busca70ocup(int nbuckets);
int teste_busca80ocup(int nbuckets);
int teste_busca90ocup(int nbuckets);
int teste_busca99ocup(int nbuckets);



/* TESTES DE BUSCA */

void teste_busca(){
    int nbuckets = 6100;
    printf("Testando buscas com diferentes taxas de ocupação...\n");
    
    clock_t start, end;
    double cpu_time_used;
    
    start = clock();
    assert(teste_busca10ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 10%%: %.4f segundos\n", cpu_time_used);
    
    start = clock();
    assert(teste_busca20ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 20%%: %.4f segundos\n", cpu_time_used);
    
    start = clock();
    assert(teste_busca30ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 30%%: %.4f segundos\n", cpu_time_used);

    start = clock();
    assert(teste_busca40ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 40%%: %.4f segundos\n", cpu_time_used);

    start = clock();
    assert(teste_busca50ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 50%%: %.4f segundos\n", cpu_time_used);

    start = clock();
    assert(teste_busca60ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 60%%: %.4f segundos\n", cpu_time_used);
    
    start = clock();
    assert(teste_busca70ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 70%%: %.4f segundos\n", cpu_time_used);

    start = clock();
    assert(teste_busca80ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 80%%: %.4f segundos\n", cpu_time_used);
    
    start = clock();
    assert(teste_busca90ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 90%%: %.4f segundos\n", cpu_time_used);
    
    start = clock();
    assert(teste_busca99ocup(nbuckets) == EXIT_SUCCESS);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Taxa 99%%: %.4f segundos\n", cpu_time_used);
    
    printf("Todos os testes de busca passaram com sucesso!\n");
}

void busca_padrao(thash h){
    char *key = "69927";
    tcep *resultado = (tcep *)hash_busca(h, key);

}

int teste_busca10ocup(int nbuckets){
    thash h;
    float taxaocup = 0.1;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca20ocup(int nbuckets){
    thash h;
    float taxaocup = 0.2;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca30ocup(int nbuckets){
    thash h;
    float taxaocup = 0.3;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca40ocup(int nbuckets){
    thash h;
    float taxaocup = 0.4;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca50ocup(int nbuckets){
    thash h;
    float taxaocup = 0.5;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca60ocup(int nbuckets){
    thash h;
    float taxaocup = 0.6;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca70ocup(int nbuckets){
    thash h;
    float taxaocup = 0.7;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca80ocup(int nbuckets){
    thash h;
    float taxaocup = 0.8;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca90ocup(int nbuckets){
    thash h;
    float taxaocup = 0.9;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

int teste_busca99ocup(int nbuckets){
    thash h;
    float taxaocup = 0.99;
    int resultado = constroi_dataset(&h, nbuckets, get_key, taxaocup);
    busca_padrao(h);
    hash_apaga(&h);
    return resultado;
}

/* DECLARACOES DAS FUNCOES DE TESTE INSERÇÃO */ 
void teste_insere6100buckets();
void teste_insere1000buckets();


/* TESTES DE INSERÇÃO */

void teste_insere6100buckets(){
    int nbuckets = 6100;
    thash h;
    constroi_dataset(&h, nbuckets, get_key, 0.7);
    hash_apaga(&h);
}

void teste_insere1000buckets(){
    int nbuckets = 1000;
    thash h;
    constroi_dataset(&h, nbuckets, get_key, 0.7);
    hash_apaga(&h);
}



/* MAIN */

int main(int argc, char* argv[]){

    clock_t start, end;
    double cpu_time_used;
    
    start = clock();
    teste_insere1000buckets();
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time for teste_insere1000buckets: %.4f seconds\n", cpu_time_used);
    
    start = clock();
    teste_insere6100buckets();
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time for teste_insere6100buckets: %.4f seconds\n", cpu_time_used);

    start = clock();
    teste_busca();
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time for teste_busca: %.4f seconds\n", cpu_time_used);

    return 0;
}