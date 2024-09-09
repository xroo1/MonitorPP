#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <time.h>
#include <sys/stat.h>

#define GRN "\033[1;32m"
#define RED "\033[1;31m"
#define RST "\033[0m"

// Função que imprime o banner
void banner(const char *c1, const char *c2) {
    printf("%s\n", c1);
    printf("    ▄▄▄              ▪\n");
    printf("    ▀▄ █·▪     ▪    ▄██\n");
    printf("    ▐▀▀▄  ▄█▀▄  ▄█▀▄ ▐█·\n");
    printf("    ▐█•█▌▐█▌.▐▌▐█▌.▐▌▐█▌\n");
    printf("    .▀  ▀ ▀█▄▀▪ ▀█▄▀▪▀▀▀\n");
    printf("\n       Monitor%s\n", c2);
}

// Função para obter o uso de RAM (em MB)
void get_ram_usage(long *used, long *total) {
    struct sysinfo info;
    sysinfo(&info);

    *total = info.totalram / 1024 / 1024; // Convertendo para MB
    *used = (info.totalram - info.freeram) / 1024 / 1024; // Convertendo para MB
}

// Função para obter o uso de disco (em GB)
void get_disk_usage(double *used, double *free, double *total) {
    struct statvfs stat;
    statvfs("/", &stat);

    *total = (double)stat.f_blocks * stat.f_frsize / 1024 / 1024 / 1024; // Convertendo para GB
    *free = (double)stat.f_bfree * stat.f_frsize / 1024 / 1024 / 1024;
    *used = *total - *free;
}

// Função para calcular o uso de CPU
double get_cpu_usage() {
    long double a[4], b[4];
    FILE *fp;
    double cpu_usage = 0.0;

    // Leitura inicial de /proc/stat
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        perror("Erro ao abrir /proc/stat");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "cpu %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
    fclose(fp);

    sleep(1); // Espera 1 segundo para calcular a diferença

    // Leitura subsequente de /proc/stat
    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        perror("Erro ao abrir /proc/stat");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "cpu %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
    fclose(fp);

    // Cálculo do uso de CPU
    long double total_a = a[0] + a[1] + a[2] + a[3];
    long double total_b = b[0] + b[1] + b[2] + b[3];

    if (total_b > total_a) {
        cpu_usage = ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) / (total_b - total_a);
        cpu_usage *= 100.0;
    } else {
        cpu_usage = 0.0;
    }

    return cpu_usage;
}

// Função para obter o nome do usuário pelo UID
char *get_username(uid_t uid) {
    struct passwd *pwd = getpwuid(uid);
    return (pwd != NULL) ? pwd->pw_name : "Desconhecido";
}

// Função para obter o uso de CPU e memória de um processo específico
double get_process_cpu_usage(int pid) {
    char path[256];
    FILE *fp;
    long double a[4], b[4];
    double cpu_usage = 0.0;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (fp) {
        fscanf(fp, "%*d %*s %*c %*d %*d %*d %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
        fclose(fp);

        sleep(1); // Espera 1 segundo para calcular a diferença

        fp = fopen(path, "r");
        if (fp) {
            fscanf(fp, "%*d %*s %*c %*d %*d %*d %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
            fclose(fp);

            long double total_a = a[0] + a[1] + a[2] + a[3];
            long double total_b = b[0] + b[1] + b[2] + b[3];

            if (total_b > total_a) {
                cpu_usage = ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) / (total_b - total_a);
                cpu_usage *= 100.0;
            }
        }
    }
    return cpu_usage;
}

// Função para obter o uso de memória de um processo específico
unsigned long get_process_memory_usage(int pid) {
    char path[256];
    FILE *fp;
    char line[256];
    unsigned long memory_usage = 0;

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line + 6, "%lu", &memory_usage);
                fclose(fp);
                return memory_usage / 1024; // Convertendo para MB
            }
        }
        fclose(fp);
    }
    return memory_usage;
}

// Função para monitorar os PIDs dos aplicativos
void monitor_pids(int argc, char *argv[]) {
    DIR *proc_dir;
    struct dirent *entry;
    char path[256], comm[256];
    FILE *fp;
    int appCount = argc - 1;
    char **appNames = &argv[1];

    // Coleta as informações do servidor (RAM, CPU, DISCO) antes de exibir
    long used_ram, total_ram;
    double used_disk, free_disk, total_disk, cpu_percent;
    get_ram_usage(&used_ram, &total_ram);
    get_disk_usage(&used_disk, &free_disk, &total_disk);
    cpu_percent = get_cpu_usage();

    while (1) {
        // Limpa a tela a cada iteração
        system("clear");

        // Exibe o status dos aplicativos (on/off)
        printf("Status dos aplicativos:\n");
        for (int i = 0; i < appCount; i++) {
            int found = 0;
            proc_dir = opendir("/proc");
            if (proc_dir == NULL) {
                perror("Erro ao abrir /proc");
                exit(EXIT_FAILURE);
            }
            while ((entry = readdir(proc_dir)) != NULL) {
                if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
                    snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);

                    fp = fopen(path, "r");
                    if (fp != NULL) {
                        if (fgets(comm, sizeof(comm), fp) != NULL) {
                            comm[strcspn(comm, "\n")] = 0;

                            if (strcmp(comm, appNames[i]) == 0) {
                                found = 1;
                                break;
                            }
                        }
                        fclose(fp);
                    }
                }
            }
            closedir(proc_dir);

            if (found) {
                printf("%s%s está on%s\n", GRN, appNames[i], RST);
            } else {
                printf("%s%s está off%s\n", RED, appNames[i], RST);
            }
        }

        // Exibe o status dos aplicativos (informações detalhadas)
        printf("\nDetalhes dos processos:\n");
        printf("%s%-5s %-25s %-15s %-10s %-10s%s\n", GRN, "PID", "Nome", "Usuário", "CPU%", "Memória%", RST);
        proc_dir = opendir("/proc");
        if (proc_dir == NULL) {
            perror("Erro ao abrir /proc");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < appCount; i++) {
            int found = 0;
            rewinddir(proc_dir); // Rewind directory for each application
            while ((entry = readdir(proc_dir)) != NULL) {
                if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
                    snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);

                    fp = fopen(path, "r");
                    if (fp != NULL) {
                        if (fgets(comm, sizeof(comm), fp) != NULL) {
                            comm[strcspn(comm, "\n")] = 0;

                            if (strcmp(comm, appNames[i]) == 0) {
                                found = 1;
                                break;
                            }
                        }
                        fclose(fp);
                    }
                }
            }
            if (found) {
                int pid = atoi(entry->d_name);
                double cpu_usage = get_process_cpu_usage(pid);
                unsigned long mem_usage = get_process_memory_usage(pid);

                // Obtém o nome do usuário do processo
                struct stat statbuf;
                snprintf(path, sizeof(path), "/proc/%d", pid);
                if (stat(path, &statbuf) == 0) {
                    char *username = get_username(statbuf.st_uid);

                    printf("%s%-5d %-25s %-15s %-10.2f %-10lu MB%s\n", GRN, pid, comm, username, cpu_usage, mem_usage, RST);
                }
            }
        }
        closedir(proc_dir);

        // Exibe as informações gerais do sistema
        printf("\nUso de RAM: %ld MB / %ld MB\n", used_ram, total_ram);
        printf("Uso de Disco: %.2f GB (%.2f GB livre / %.2f GB total)\n", used_disk, free_disk, total_disk);
        printf("Uso de CPU: %.2f%%\n", cpu_percent);

        sleep(5); // Intervalo entre atualizações
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <app1> <app2> ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    banner(" Monitor de Aplicativos ", "de Aplicações");

    monitor_pids(argc, argv);

    return 0;
}


