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
     float taxaocup; // Taxa de ocupacao da tabela $
     char * (*get_key)(void *);
}thash;

/* ESTRUTURA DOS CEPS */

typedef struct { 
    char cep_ini[6];
    char cep_fim[6];
    char cidade[50];
    char estado[3];
} tcep; // Mudanca da estrutura conforme o dataset $

/* DECLARACOES DAS FUNCOES TABELA HASH*/

uint32_t hashf(const char* str, uint32_t h);
int hash_constroi(thash * h,int nbuckets, char * (*get_key)(void *), float taxaocup);
int hash_insere(thash * h, void * bucket);
void hash_duplicar(thash *h);


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

int hash_constroi(thash * h,int nbuckets, char * (*get_key)(void *), float taxaocup){ // Mudanca para armazenar a taxa de ocupacao na tabela $
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

int hash_insere(thash * h, void * bucket){ // Mudanca para duplicar o tamanho ao atingir a ocupacao $
    float ocupacao = (float)(h->size+1) / (float)h->max;
    if (ocupacao >= h->taxaocup) // Checa taxa de ocupacao
        hash_duplicar(h); // Chama duplicacao

    uint32_t hash = hashf(h->get_key(bucket),SEED);
    int pos = hash % (h->max);
    
    while((h->table[pos]) != 0 ){
        if (h->table[pos] == h->deleted)
            break;
        pos = (pos+1) % h->max;
    }
    h->table[pos] = (uintptr_t) bucket;
    h->size += 1;
    
    return EXIT_SUCCESS;
}

void hash_duplicar(thash *h){ // Funcao que duplica o tamanho da hash $

    uintptr_t * tabela_anterior = h->table;
    int max_anterior = h->max;
    h->max *= 2; // Duplica a capacidade
    h->table = calloc(sizeof(void *),h->max); // Aloca a nova tabela

    if (h->table == NULL){ // Checa o suceso
        fprintf(stderr, "Erro ao duplicar a tabela hash\n");
        exit(EXIT_FAILURE);
    }
    h->size = 0;
    for (int i = 0; i < max_anterior; i++){ // Insere os valores da antiga tabela
        if (tabela_anterior[i] != 0 && tabela_anterior[i] != h->deleted){
            hash_insere(h, (void *)tabela_anterior[i]);
        }
    }
    free(tabela_anterior); // libera tabela anterior
}

void * hash_busca(thash h, const char * key){
    int pos = hashf(key,SEED) % (h.max);
    while(h.table[pos] != 0){
        if (strcmp(h.get_key((void *)h.table[pos]),key) == 0){
            return (void *)h.table[pos]; 
        }else
            pos = (pos+1)%h.max;
    }
    return NULL;

}

int hash_remove(thash * h, const char * key){
    int pos = hashf(key,SEED) % h->max;
    while(h->table[pos]!=0){
        if (strcmp(h->get_key((void *)h->table[pos]),key) == 0){ 
            free((void *)h->table[pos] );
            h->table[pos] = h->deleted;
            h->size -=1;
            return EXIT_SUCCESS; 
        }else
            pos = (pos+1)%h->max;
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

/* FUNCOES ESTRUTURA DOS CEPS */

char * get_key(void * reg){ 
    return ((tcep *)reg)->cep_ini;  // Mudanca da chave de estrutura $
}

void * aloca_cep(char * cep_ini, char *cep_fim, char * cidade, char * estado){ // Mudanca para condizer com a nova estrutura $
    tcep *_cep = (tcep *)malloc(sizeof(tcep));
    strcpy(_cep->cep_ini,cep_ini);
    strcpy(_cep->cep_fim,cep_fim);
    strcpy(_cep->cidade,cidade);
    strcpy(_cep->estado,estado);
    return _cep;
}

tcep * busca_cidade_por_cep(thash h, const char * cep_consultado) {
    // Converte CEP consultado para numero para comparacao
    int cep_num = atoi(cep_consultado);
    
    // Busca linear na hash (ja que nao sabemos o cep_ini exato)
    for (int i = 0; i < h.max; i++) {
        if (h.table[i] != 0 && h.table[i] != h.deleted) {
            tcep * registro = (tcep *)h.table[i];
            int cep_ini_num = atoi(registro->cep_ini);
            int cep_fim_num = atoi(registro->cep_fim);
            
            // Verifica se o CEP esta no intervalo
            if (cep_num >= cep_ini_num && cep_num <= cep_fim_num) {
                return registro;
            }
        }
    }
    return NULL;
}

/* FUNCOES DATASET */

void ler_CSV(FILE *file, thash *h) { // Funcao que permite a leitura dos dados do dataset $
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

//$
int constroi_dataset(thash * h,int nbuckets, char * (*get_key)(void *), float taxaocup){
    if (hash_constroi(h, nbuckets, get_key, taxaocup) == EXIT_FAILURE) { // Constroi uma arvore com especificacoes dadas
        fprintf(stderr, "Erro ao construir a tabela hash\n");
        return EXIT_FAILURE;
    }   
    FILE *file = fopen("ceps.csv", "r"); // Abre o arquivo
    if (!file) {
        fprintf(stderr, "Erro ao abrir o arquivo cep.csv\n");
        return EXIT_FAILURE;
    }
    ler_CSV(file, h); // Chama ler_CSV que ja adiciona os dados do dataset a tabela
    fclose(file);

    return EXIT_SUCCESS;
}

/* COMPARATIVOS */

void busca10(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.1);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca20(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.2);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca30(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.3);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca40(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.4);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca50(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.5);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca60(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.6);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}
void busca70(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.7);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca80(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.8);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca90(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.9);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
}

void busca99(const char * cep){
    thash h;
    constroi_dataset(&h, 6100, get_key, 0.99);
    tcep *resultado = (tcep *)hash_busca(h, cep);
    if (resultado) {
        printf("CEP encontrado: %s - %s, %s/%s\n", resultado->cep_ini, resultado->cidade, resultado->estado, resultado->cep_fim);
    }
    else {
        printf("CEP %s nao encontrado.\n", cep);
    }
    hash_apaga(&h);
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


int main(int argc, char* argv[]){
    char *cep = "76510";

    //busca99(cep);

    /*
    clock_t start, end;
    double cpu_time_used;

    start = clock();
    busca10(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca10: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca20(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca20: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca30(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca30: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca40(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca40: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca50(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca50: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca60(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca60: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca70(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca70: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca80(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca80: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca90(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca90: %.4f seconds\n", cpu_time_used);

    start = clock();
    busca99(cep);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("busca99: %.4f seconds\n", cpu_time_used);
    */

    // /*
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
    // */
    
    return EXIT_SUCCESS;
}