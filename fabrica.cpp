#include "fabrica.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>

// Fila de espera
void FilaEspera::enqueue(int idVeiculo, const std::string& tarefa) {
    fila.push({idVeiculo, tarefa});
}

std::pair<int, std::string> FilaEspera::dequeue() {
    auto front = fila.front();
    fila.pop();
    return front;
}

bool FilaEspera::empty() const {
    return fila.empty();
}

// Gerenciador de Robos
GerenciadorRobos::GerenciadorRobos(int n) {
    for (int i = 1; i <= n; ++i)
        livres.push(i);
}

int GerenciadorRobos::reservar() {
    if (livres.empty()) return -1;
    int id = livres.front();
    livres.pop();
    return id;
}

void GerenciadorRobos::liberar(int idRobo) {
    livres.push(idRobo);
}

bool GerenciadorRobos::temLivre() const {
    return !livres.empty();
}

// Processo filho — grava Robo_X.txt
//
// Comunicação via pipe: pai escreve mensagens, filho lê e grava no arquivo
//   VEICULO <id>    = abre uma nova seção no log
//   TAREFA <desc>   = registra tarefa
//   FIM_VEICULO     = Salva alterações e aguarda novo veículo
//   ENCERRAR        = fecha arquivo e termina

void rodarFilho(int idRobo, int fdLeitura) {
    std::string nomeArq = "Robo_" + std::to_string(idRobo) + ".txt";
    std::ofstream log(nomeArq);
    log << "Robo_" << idRobo << "\n";

    FILE* pipe = fdopen(fdLeitura, "r");
    if (!pipe) _exit(1);

    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::string linha(buf);
        if (!linha.empty() && linha.back() == '\n')
            linha.pop_back();

        if (linha.rfind("VEICULO ", 0) == 0) {
            log << "\nVeiculo " << linha.substr(8) << ":\n";
        } else if (linha.rfind("TAREFA ", 0) == 0) {
            log << "- " << linha.substr(7) << "\n";
        } else if (linha == "FIM_VEICULO") {
            log.flush();
        } else if (linha == "ENCERRAR") {
            break;
        }
    }

    log.close();
    fclose(pipe);
    _exit(0);
}

// Sistema Central

SistemaCentral::SistemaCentral(int n, int m)
    : n(n), m(m), gerRobos(n), processos(n + 1)
{
    for (int i = 1; i <= n; ++i)
        criarProcessoFilho(i);
}

SistemaCentral::~SistemaCentral() {
    encerrarFilhos();
    aguardarFilhos();
}

void SistemaCentral::criarProcessoFilho(int idRobo) {
    int fds[2];
    if (pipe(fds) == -1)
        throw std::runtime_error("Erro ao criar pipe");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("Erro ao fazer fork");

    if (pid == 0) {
        // filho fecha escrita, roda
        close(fds[1]);
        rodarFilho(idRobo, fds[0]);
        // nunca retorna, garante que processo filho termine
    }

    // pai fecha leitura, guarda escrita
    close(fds[0]);
    processos[idRobo] = {idRobo, pid, fds[1]};
}

void SistemaCentral::enviarMensagem(int idRobo, const std::string& msg
) {
    std::string linha = msg + "\n";
    write(processos[idRobo].fdEscrita, linha.c_str(), linha.size());
}

void SistemaCentral::atribuirVeiculo(int idVeiculo, const std::string& tarefa, int idRobo) {
    veiculosAtivos[idVeiculo] = {idRobo, processos[idRobo].fdEscrita};
    enviarMensagem(idRobo, "VEICULO " + std::to_string(idVeiculo));
    enviarMensagem(idRobo, "TAREFA "  + tarefa);
}

void SistemaCentral::handleTarefa(int idVeiculo, const std::string& tarefa) {
    auto it = veiculosAtivos.find(idVeiculo);
    if (it != veiculosAtivos.end()) {
        // veículo já em atendimento = enviar tarefa direto
        enviarMensagem(it->second.idRobo, "TAREFA " + tarefa);
    } else {
        int idRobo = gerRobos.reservar(); // O(1)
        if (idRobo != -1) {
            atribuirVeiculo(idVeiculo, tarefa, idRobo);
        } else {
            filaEspera.enqueue(idVeiculo, tarefa); // sem robô = fila
        }
    }
}

void SistemaCentral::handleFim(int idVeiculo) {
    auto it = veiculosAtivos.find(idVeiculo);
    if (it == veiculosAtivos.end()) return;

    int idRobo = it->second.idRobo;
    enviarMensagem(idRobo, "FIM_VEICULO");
    veiculosAtivos.erase(it);
    gerRobos.liberar(idRobo); // O(1)
    processarFila();
}

void SistemaCentral::processarFila() {
    while (!filaEspera.empty() && gerRobos.temLivre()) {
        auto [idVeiculo, tarefa] = filaEspera.dequeue();
        atribuirVeiculo(idVeiculo, tarefa, gerRobos.reservar());
    }
}

void SistemaCentral::encerrarFilhos() {
    for (int i = 1; i <= n; ++i) {
        if (processos[i].fdEscrita != -1) {
            enviarMensagem(i, "ENCERRAR");
            close(processos[i].fdEscrita);
            processos[i].fdEscrita = -1;
        }
    }
}

void SistemaCentral::aguardarFilhos() {
    for (int i = 1; i <= n; ++i) {
        if (processos[i].pid > 0) {
            waitpid(processos[i].pid, nullptr, 0);
            processos[i].pid = 0;
        }
    }
}

void SistemaCentral::executar() {
    std::string linha;
    while (std::getline(std::cin, linha)) {
        if (linha == "FIM") break;
        if (linha.empty()) continue;

        std::istringstream ss(linha);
        std::string token;
        ss >> token;

        int idVeiculo;
        try { idVeiculo = std::stoi(token); }
        catch (...) { continue; }

        std::string resto;
        std::getline(ss, resto);
        if (!resto.empty() && resto.front() == ' ')
            resto = resto.substr(1);

        if (resto == "fim")
            handleFim(idVeiculo);
        else
            handleTarefa(idVeiculo, resto);
    }

    encerrarFilhos();
    aguardarFilhos();
}
