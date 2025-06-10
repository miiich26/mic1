#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tipos
typedef unsigned char byte;
typedef unsigned int palavra;
typedef unsigned long int microinstrucao;

// Constantes
#define TAM_MEM 100000000
#define TAM_ARMAZENAMENTO 512

// Registradores
typedef struct {
    palavra MAR, MDR, PC;
    byte MBR;
    palavra SP, LV, TOS, OPC, CPP, H;
    palavra MPC;
    microinstrucao MIR;
} Registradores;

Registradores regs;

// Sinais de Controle
typedef struct {
    byte B, Operacao, Deslocador, MEM, Pulo;
    palavra C;
} SinaisControle;

SinaisControle sinal;

// Memória e armazenamento de microprograma
byte Memoria[TAM_MEM];
microinstrucao Armazenamento[TAM_ARMAZENAMENTO];

// Barramentos
palavra busB, busC;

// Flags
byte N, Z;

// Prototipação
void carregar_microprograma();
void carregar_programa(const char *arquivo);
void decodificar_microinstrucao();
void atribuir_barramento_B();
void realizar_operacao_ALU();
void atribuir_barramento_C();
void operar_memoria();
void pular();
void exibir_estado();
void print_bin(void *valor, size_t bytes);

// Função principal
int main(int argc, const char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s programa.bin\n", argv[0]);
        return EXIT_FAILURE;
    }

    carregar_microprograma();
    carregar_programa(argv[1]);

    while (1) {
        exibir_estado();

        regs.MIR = Armazenamento[regs.MPC];

        decodificar_microinstrucao();
        atribuir_barramento_B();
        realizar_operacao_ALU();
        atribuir_barramento_C();
        operar_memoria();
        pular();
    }

    return 0;
}

// Implementações
void carregar_microprograma() {
    FILE *arquivo = fopen("microprog.rom", "rb");
    if (!arquivo) {
        perror("Erro ao abrir microprog.rom");
        exit(EXIT_FAILURE);
    }

    fread(Armazenamento, sizeof(microinstrucao), TAM_ARMAZENAMENTO, arquivo);
    fclose(arquivo);
}

void carregar_programa(const char *arquivo_nome) {
    FILE *arquivo = fopen(arquivo_nome, "rb");
    if (!arquivo) {
        perror("Erro ao abrir o programa");
        exit(EXIT_FAILURE);
    }

    byte tamanho_bytes[4];
    if (fread(tamanho_bytes, sizeof(byte), 4, arquivo) != 4) {
        fprintf(stderr, "Erro ao ler tamanho do programa.\n");
        fclose(arquivo);
        exit(EXIT_FAILURE);
    }

    palavra tamanho;
    memcpy(&tamanho, tamanho_bytes, 4);

    fread(Memoria, sizeof(byte), 20, arquivo);
    fread(&Memoria[0x401], sizeof(byte), tamanho - 20, arquivo);

    fclose(arquivo);
}

void decodificar_microinstrucao() {
    microinstrucao mir = regs.MIR;
    sinal.B          = (mir)        & 0b1111;
    sinal.MEM        = (mir >> 4)   & 0b111;
    sinal.C          = (mir >> 7)   & 0x1FF;
    sinal.Operacao   = (mir >> 16)  & 0b111111;
    sinal.Deslocador = (mir >> 22)  & 0b11;
    sinal.Pulo       = (mir >> 24)  & 0b111;
    regs.MPC         = (mir >> 27)  & 0x1FF;
}

palavra ler_registrador(byte codigo) {
    switch (codigo) {
        case 0: return regs.MDR;
        case 1: return regs.PC;
        case 2: return (regs.MBR & 0x80) ? (regs.MBR | 0xFFFFFF00) : regs.MBR;
        case 3: return regs.MBR;
        case 4: return regs.SP;
        case 5: return regs.LV;
        case 6: return regs.CPP;
        case 7: return regs.TOS;
        case 8: return regs.OPC;
        default: return 0;
    }
}

void atribuir_barramento_B() {
    busB = ler_registrador(sinal.B);
}

void realizar_operacao_ALU() {
    switch (sinal.Operacao) {
        case 12: busC = regs.H & busB; break;
        case 17: busC = 1; break;
        case 18: busC = -1; break;
        case 20: busC = busB; break;
        case 24: busC = regs.H; break;
        case 26: busC = ~regs.H; break;
        case 28: busC = regs.H | busB; break;
        case 44: busC = ~busB; break;
        case 53: busC = busB + 1; break;
        case 54: busC = busB - 1; break;
        case 57: busC = regs.H + 1; break;
        case 59: busC = -regs.H; break;
        case 60: busC = regs.H + busB; break;
        case 61: busC = regs.H + busB + 1; break;
        case 63: busC = busB - regs.H; break;
        default: busC = 0; break;
    }

    Z = (busC == 0);
    N = !Z;

    switch (sinal.Deslocador) {
        case 1: busC <<= 8; break;
        case 2: busC >>= 1; break;
    }
}

void atribuir_barramento_C() {
    if (sinal.C & 0x001) regs.MAR = busC;
    if (sinal.C & 0x002) regs.MDR = busC;
    if (sinal.C & 0x004) regs.PC  = busC;
    if (sinal.C & 0x008) regs.SP  = busC;
    if (sinal.C & 0x010) regs.LV  = busC;
    if (sinal.C & 0x020) regs.CPP = busC;
    if (sinal.C & 0x040) regs.TOS = busC;
    if (sinal.C & 0x080) regs.OPC = busC;
    if (sinal.C & 0x100) regs.H   = busC;
}

void operar_memoria() {
    if (sinal.MEM & 0b001) regs.MBR = Memoria[regs.PC];
    if (sinal.MEM & 0b010) memcpy(&regs.MDR, &Memoria[regs.MAR * 4], 4);
    if (sinal.MEM & 0b100) memcpy(&Memoria[regs.MAR * 4], &regs.MDR, 4);
}

void pular() {
    if (sinal.Pulo & 0b001) regs.MPC |= (N << 8);
    if (sinal.Pulo & 0b010) regs.MPC |= (Z << 8);
    if (sinal.Pulo & 0b100) regs.MPC |= regs.MBR;
}

void exibir_estado() {
    printf("\n==== REGISTRADORES ====\n");
    printf("MAR: "); print_bin(&regs.MAR, 4); printf("\n");
    printf("MDR: "); print_bin(&regs.MDR, 4); printf("\n");
    printf("PC : "); print_bin(&regs.PC, 4); printf("\n");
    printf("MBR: "); print_bin(&regs.MBR, 1); printf("\n");
    printf("SP : "); print_bin(&regs.SP, 4); printf("\n");
    printf("LV : "); print_bin(&regs.LV, 4); printf("\n");
    printf("CPP: "); print_bin(&regs.CPP, 4); printf("\n");
    printf("TOS: "); print_bin(&regs.TOS, 4); printf("\n");
    printf("OPC: "); print_bin(&regs.OPC, 4); printf("\n");
    printf("H  : "); print_bin(&regs.H, 4); printf("\n");
    printf("MPC: "); print_bin(&regs.MPC, 4); printf("\n");
    printf("MIR: "); print_bin(&regs.MIR, 8); printf("\n");
    getchar();
}

void print_bin(void *valor, size_t bytes) {
    byte *data = (byte *)valor;
    for (int i = bytes - 1; i >= 0; i--) {
        for (int j = 7; j >= 0
