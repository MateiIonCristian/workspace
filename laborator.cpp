#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> // Pentru atoi

// Presupunem ca acestea sunt definite in Global Scope
#define NUM_PROCESSES 10
#define MAX_NUMBER 100000
#define RANGE_SIZE (MAX_NUMBER / NUM_PROCESSES)

using namespace std;

// Functia care verifica daca un numar este prim
bool IsPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

// Inceputul programului Părinte
int main(int argc, char* argv[]) {

    // --- LOGICA COPIILOR (Rulare pe interval) ---
    if (argc == 3) {
        // Cazul in care procesul este un COPLIL (ruleaza calculul de numere prime pe un interval)
        int start = atoi(argv[1]);
        int end = atoi(argv[2]);

        for (int n = start; n <= end; n++) {
            if (IsPrime(n)) {
                // Afiseaza numarul prim urmat de un separator
                cout << n << " ";
            }
        }
        return 0; // Copilul termina
    }

    // --- LOGICA PARINTELUI (Creare procese si citire din pipe) ---

    cout << "Numere prime intre 1 si " << MAX_NUMBER << " : \n";

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        // 1. Pregatirea Pipe-ului
        HANDLE childStd_OUT_Rd = NULL;
        HANDLE childStd_OUT_Wr = NULL;

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE; // Handle-ul de scriere (Wr) trebuie sa fie mostenibil!
        saAttr.lpSecurityDescriptor = NULL;

        // Creeaza pipe-ul anonim.
        if (!CreatePipe(&childStd_OUT_Rd, &childStd_OUT_Wr, &saAttr, 0)) {
            cerr << "Eroare la crearea pipe-ului.\n";
            return 1;
        }

        // Asigura-te ca handle-ul de citire al parintelui NU este mostenibil.
        if (!SetHandleInformation(childStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
            cerr << "Eroare la SetHandleInformation.\n";
            CloseHandle(childStd_OUT_Rd);
            CloseHandle(childStd_OUT_Wr);
            return 1;
        }

        // 2. Pregatirea structurilor de lansare
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        // Seteaza standard output-ul copilului sa fie capatul de scriere (Wr) al pipe-ului
        si.hStdError = childStd_OUT_Wr;
        si.hStdOutput = childStd_OUT_Wr;
        si.dwFlags |= STARTF_USESTDHANDLES; // Activeaza folosirea handle-urilor de I/O specificate

        ZeroMemory(&pi, sizeof(pi));

        // 3. Calculul intervalului pentru copil
        int start = i * RANGE_SIZE + 1;
        int end = (i + 1) * RANGE_SIZE;
        if (end > MAX_NUMBER) end = MAX_NUMBER;

        // 4. Crearea liniei de comanda
        string cmdLine_str = string(argv[0]) + " " + to_string(start) + " " + to_string(end);

        // Copiem șirul în vector<char> pentru a obține un buffer modificabil (LPSTR)
        // Aceasta este o metodă mai robustă decât const_cast
        std::vector<char> cmdLine(cmdLine_str.begin(), cmdLine_str.end());
        cmdLine.push_back('\0'); // Adaugati terminatorul null

        // 5. Crearea Procesului Copil
        if (!CreateProcessA(
            NULL,                      // LPCTSTR lpApplicationName (se foloseste cmdLine)
            cmdLine.data(),            // LPTSTR lpCommandLine (Pointer la buffer modificabil - char*)
            NULL,                      // LPSECURITY_ATTRIBUTES lpProcessAttributes
            NULL,                      // LPSECURITY_ATTRIBUTES lpThreadAttributes
            TRUE,                      // BOOL bInheritHandles (Copilul mosteneste handle-ul de scriere (Wr))
            0,                         // DWORD dwCreationFlags
            NULL,                      // LPVOID lpEnvironment
            NULL,                      // LPCTSTR lpCurrentDirectory
            &si,                       // LPSTARTUPINFO
            &pi                        // LPPROCESS_INFORMATION
        )) {
            cerr << "Eroare la crearea procesului copil " << i << ". Cod: " << GetLastError() << "\n";
            CloseHandle(childStd_OUT_Rd);
            CloseHandle(childStd_OUT_Wr);
            continue;
        }

        // 6. Citirea din Pipe (in Parinte)
        // Parintele trebuie sa inchida capatul de scriere al pipe-ului (Wr) dupa ce a lansat copilul.
        CloseHandle(childStd_OUT_Wr); // Nu mai scriem in pipe din parinte

        CHAR buffer[256];
        DWORD bytesRead;
        BOOL success = FALSE;

        // Citeste continuu din capatul de citire (Rd) al pipe-ului
        while (true) {
            success = ReadFile(childStd_OUT_Rd, buffer, sizeof(buffer) - 1, &bytesRead, NULL);

            if (!success || bytesRead == 0) {
                break; // Iesire daca ReadFile esueaza sau citeste 0 bytes (pipe-ul s-a inchis)
            }

            buffer[bytesRead] = '\0'; // Terminare sir de caractere
            cout << buffer;           // Afiseaza direct rezultatul citit
        }

        // 7. Curatare dupa Copil
        CloseHandle(childStd_OUT_Rd); // Inchide capatul de citire al pipe-ului

        // Asteapta terminarea procesului copil
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Inchide handle-urile procesului si firului (pentru a elibera resursele kernel)
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    cout << "\nProgramul parinte a terminat.\n";
    return 0;
}