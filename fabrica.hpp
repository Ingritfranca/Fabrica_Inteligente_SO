#pragma once
#include <queue>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>
#include <sys/types.h>

//Fila de espera para veículos sem robô livre enqueue/dequeue em O(1)
class FilaEspera {
public:
    void enqueue(int idVeiculo, const std::string& tarefa);
    std::pair<int, std::string> dequeue();
    bool empty() const;
private:
    std::queue<std::pair<int, std::string>> fila;
};


// Pool de robôs reserva e liberação em O(1)
class GerenciadorRobos {
public:
    explicit GerenciadorRobos(int n);
    int  reservar();  // retorna id do robô ou -1
    void liberar(int idRobo);
    bool temLivre() const;
private:
    std::queue<int> livres;
};

// Dados de um processo filho (robô)

struct ProcessoRobo {
    int   idRobo    = 0;
    pid_t pid       = 0;
    int   fdEscrita = -1; // lado de escrita do pipe (pai usa)
};

// Orquestrador principal
class SistemaCentral {
public:
    SistemaCentral(int n, int m);
    ~SistemaCentral();
    void executar();

private:
    int n, m;
    GerenciadorRobos gerRobos;
    FilaEspera       filaEspera;

    struct InfoVeiculo { int idRobo; int fdEscrita; };
    std::unordered_map<int, InfoVeiculo> veiculosAtivos; // consulta O(1)

    std::vector<ProcessoRobo> processos; // índices 1..n

    void criarProcessoFilho(int idRobo);
    void enviarMensagem(int idRobo, const std::string& msg);
    void atribuirVeiculo(int idVeiculo, const std::string& tarefa, int idRobo);
    void handleTarefa(int idVeiculo, const std::string& tarefa);
    void handleFim(int idVeiculo);
    void processarFila();
    void encerrarFilhos();
    void aguardarFilhos();
};

// Função executada pelo processo filho
void rodarFilho(int idRobo, int fdLeitura);
