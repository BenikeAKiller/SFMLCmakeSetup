#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <cmath>

#include "imgui.h"
#include "imgui-SFML.h"

struct QuizQuestion {
    std::string question;
    char correctSound;
    char answer;
    bool swapped = false;
    sf::SoundBuffer bufferA;
    sf::SoundBuffer bufferB;

    // Peak Hold Data
    std::vector<ImVec2> widestFrameA;
    float maxWidthScoreA = 0.0f;
    std::vector<ImVec2> widestFrameB;
    float maxWidthScoreB = 0.0f;
};

struct ErrorStat {
    int id;
    int count;
};

enum class AppState { Loading, Quiz, Results };

// --- File & Stats Helpers ---

std::string GetResultsFilePath() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    return home ? std::string(home) + "\\Documents\\quiz_results.csv" : "quiz_results.csv";
#else
    return "quiz_results.csv";
#endif
}

std::string GetResultsFolderPath() {
    return std::filesystem::path(GetResultsFilePath()).parent_path().string();
}

int GetNextAttemptNumber() {
    std::ifstream file(GetResultsFilePath());
    if (!file.is_open()) return 1;
    int lastAttempt = 0;
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        size_t commaPos = line.find(',');
        if (commaPos != std::string::npos) {
            try {
                int attempt = std::stoi(line.substr(0, commaPos));
                if (attempt > lastAttempt) lastAttempt = attempt;
            }
            catch (...) {}
        }
    }
    return lastAttempt + 1;
}

std::vector<ErrorStat> GetGlobalErrorStats(int totalQuestions) {
    std::vector<ErrorStat> stats(totalQuestions);
    for (int i = 0; i < totalQuestions; i++) stats[i] = { i + 1, 0 };
    std::ifstream file(GetResultsFilePath());
    if (!file.is_open()) return stats;
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        size_t lastComma = line.find_last_of(',');
        if (lastComma == std::string::npos) continue;
        std::stringstream ss(line.substr(lastComma + 1));
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            try {
                int qIdx = std::stoi(segment) - 1;
                if (qIdx >= 0 && qIdx < totalQuestions) stats[qIdx].count++;
            }
            catch (...) {}
        }
    }
    return stats;
}

void AppendResultsToCSV(const std::vector<QuizQuestion>& quiz, const std::vector<char>& playerChoices) {
    int totalQuestions = quiz.size();
    int score = 0;
    for (int i = 0; i < totalQuestions; i++) if (playerChoices[i] == quiz[i].answer) score++;
    int attempt = GetNextAttemptNumber();
    std::ofstream file(GetResultsFilePath(), std::ios::app);
    if (!file.is_open()) return;
    file << attempt << "," << (float)score / totalQuestions * 100.0f << "," << score << "," << totalQuestions << ",";
    bool first = true;
    for (int i = 0; i < totalQuestions; i++) {
        if (playerChoices[i] != quiz[i].answer) {
            if (!first) file << ";";
            file << (i + 1);
            first = false;
        }
    }
    file << "\n";
}

// --- Visualizers ---

void DrawStereoMeter(sf::Sound& sound, const sf::SoundBuffer& buffer, std::vector<ImVec2>& widestFrame, float& maxStoredWidth) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float size = 60.0f;
    ImVec2 center = ImVec2(pos.x + size / 2, pos.y + size / 2);

    draw_list->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), IM_COL32(15, 15, 15, 255));
    draw_list->AddRect(pos, ImVec2(pos.x + size, pos.y + size), IM_COL32(80, 80, 80, 255));

    for (const auto& pt : widestFrame) {
        draw_list->AddCircleFilled(ImVec2(center.x + pt.x, center.y + pt.y), 1.0f, IM_COL32(180, 100, 255, 60));
    }

    if (sound.getStatus() == sf::Sound::Playing) {
        sf::Time offset = sound.getPlayingOffset();
        const sf::Int16* samples = buffer.getSamples();
        unsigned int channels = buffer.getChannelCount();
        size_t cur = static_cast<size_t>(offset.asSeconds() * buffer.getSampleRate() * channels);

        if (channels >= 2) {
            const int windowSize = 800;
            float peak = 0.001f;
            float currentWidthScore = 0.0f;
            std::vector<ImVec2> currentPoints;

            static float lpSide = 0.0f;
            const float alpha = 0.25f;

            for (int i = 0; i < windowSize; i += 2) {
                if (cur + i + 1 < buffer.getSampleCount()) {
                    float L = samples[cur + i] / 32768.0f;
                    float R = samples[cur + i + 1] / 32768.0f;
                    float mid = (L + R);
                    float sideRaw = (L - R);
                    lpSide = lpSide + alpha * (sideRaw - lpSide);

                    if (std::abs(L) > peak) peak = std::abs(L);
                    if (std::abs(R) > peak) peak = std::abs(R);

                    float norm = (size / 2.3f) / peak;
                    float x = lpSide * norm;
                    float y = -mid * norm;

                    if (i % 4 == 0) {
                        currentPoints.push_back(ImVec2(x, y));
                        draw_list->AddCircleFilled(ImVec2(center.x + x, center.y + y), 1.2f, IM_COL32(0, 255, 230, 180));
                        currentWidthScore += std::abs(lpSide);
                    }
                }
            }
            if (currentWidthScore > maxStoredWidth) {
                maxStoredWidth = currentWidthScore;
                widestFrame = currentPoints;
            }
        }
    }
    ImGui::Dummy(ImVec2(size, size));
}

int main() {
    sf::RenderWindow window(sf::VideoMode(1280, 720), "Which Sound Is Wider?");
    window.setVerticalSyncEnabled(true);
    ImGui::SFML::Init(window);
    ImGui::GetIO().FontGlobalScale = 1.6f;
    sf::Clock deltaClock;

    std::string qText[] = { "Which sound is wider?", "Which sound is wider?", "Which sound is wider?", "Which sound is wider?", "Which sound is wider?", "Which sound is wider?" };
    char cSounds[] = { 'A','A','A','A','A','A' };
    int totalQuestions = 6;
    std::vector<QuizQuestion> quiz(totalQuestions);
    for (int i = 0; i < totalQuestions; i++) { quiz[i].question = qText[i]; quiz[i].correctSound = cSounds[i]; }

    std::random_device rd; std::mt19937 rng(rd());
    std::shuffle(quiz.begin(), quiz.end(), rng);

    std::vector<sf::Sound> soundsA(totalQuestions), soundsB(totalQuestions);
    AppState state = AppState::Loading;
    int loadingIndex = 0, currentQuestion = 0;
    std::vector<char> playerChoices(totalQuestions, 0);

    // DEBUG STATE
    bool debugMode = false;
    bool showDebugEntry = false;
    char debugPass[64] = "";

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1260, 700), ImGuiCond_FirstUseEver);
        ImGui::Begin("Wider Sound Quiz", nullptr, ImGuiWindowFlags_None);

        if (state == AppState::Loading) {
            if (loadingIndex < totalQuestions) {
                std::string pA = std::string(RESOURCES_PATH) + "FlAC_for_quiz/sound" + std::to_string(loadingIndex + 1) + "_A.flac";
                std::string pB = std::string(RESOURCES_PATH) + "FlAC_for_quiz/sound" + std::to_string(loadingIndex + 1) + "_B.flac";
                quiz[loadingIndex].bufferA.loadFromFile(pA);
                quiz[loadingIndex].bufferB.loadFromFile(pB);
                bool swap = std::uniform_int_distribution<int>(0, 1)(rng);
                if (swap) std::swap(quiz[loadingIndex].bufferA, quiz[loadingIndex].bufferB);
                quiz[loadingIndex].answer = (quiz[loadingIndex].correctSound == 'A') ? (swap ? 'B' : 'A') : (swap ? 'A' : 'B');
                soundsA[loadingIndex].setBuffer(quiz[loadingIndex].bufferA);
                soundsB[loadingIndex].setBuffer(quiz[loadingIndex].bufferB);
                loadingIndex++;
            }
            else state = AppState::Quiz;
            ImGui::ProgressBar(loadingIndex / (float)totalQuestions);
        }
        else if (state == AppState::Quiz) {
            ImGui::Text("Question %d of %d", currentQuestion + 1, totalQuestions);

            // --- DEBUG SECTION ---
            if (!debugMode) {
                ImGui::Checkbox("Enable Debug", &showDebugEntry);
                if (showDebugEntry) {
                    ImGui::SameLine();
                    ImGui::InputText("Password", debugPass, 64, ImGuiInputTextFlags_Password);
                    if (std::string(debugPass) == "debug") {
                        debugMode = true;
                        memset(debugPass, 0, sizeof(debugPass)); // Clear pass
                    }
                }
            }
            else {
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "DEBUG MODE ON | Correct Answer: %c", quiz[currentQuestion].answer);
            }
            // ---------------------

            ImGui::Separator();
            if (ImGui::Button("Play A", ImVec2(200, 60))) { soundsB[currentQuestion].stop(); soundsA[currentQuestion].play(); }
            ImGui::SameLine();
            if (ImGui::Button("Play B", ImVec2(200, 60))) { soundsA[currentQuestion].stop(); soundsB[currentQuestion].play(); }

            ImGui::Separator();
            if (ImGui::Button("Answer A", ImVec2(300, 80))) { playerChoices[currentQuestion] = 'A'; currentQuestion++; }
            ImGui::SameLine();
            if (ImGui::Button("Answer B", ImVec2(300, 80))) { playerChoices[currentQuestion] = 'B'; currentQuestion++; }
            if (currentQuestion >= totalQuestions) state = AppState::Results;
        }
        else if (state == AppState::Results) {
            int score = 0;
            for (int i = 0; i < totalQuestions; i++) if (playerChoices[i] == quiz[i].answer) score++;
            ImGui::Text("Final Accuracy: %.1f%% (%d/%d)", (float)score / totalQuestions * 100.0f, score, totalQuestions);

            if (ImGui::Button("Export CSV")) AppendResultsToCSV(quiz, playerChoices);
            ImGui::SameLine();
            if (ImGui::Button("Open Folder")) {
#ifdef _WIN32
                std::string cmd = "explorer \"" + GetResultsFolderPath() + "\""; system(cmd.c_str());
#endif
            }

            ImGui::Separator();
            ImGui::Text("Review (Purple = Widest Recorded Peak):");

            if (ImGui::BeginChild("Review", ImVec2(0, 300), true)) {
                for (int i = 0; i < totalQuestions; i++) {
                    ImGui::PushID(i);
                    bool correct = (playerChoices[i] == quiz[i].answer);
                    ImGui::TextColored(correct ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), "Q%d", i + 1);
                    ImGui::SameLine();
                    if (ImGui::Button("Play A")) { soundsB[i].stop(); soundsA[i].play(); }
                    ImGui::SameLine();
                    DrawStereoMeter(soundsA[i], quiz[i].bufferA, quiz[i].widestFrameA, quiz[i].maxWidthScoreA);
                    ImGui::SameLine();
                    if (ImGui::Button("Play B")) { soundsA[i].stop(); soundsB[i].play(); }
                    ImGui::SameLine();
                    DrawStereoMeter(soundsB[i], quiz[i].bufferB, quiz[i].widestFrameB, quiz[i].maxWidthScoreB);
                    ImGui::SameLine();
                    ImGui::Text("| Choice: %c | Correct: %c", playerChoices[i], quiz[i].answer);
                    ImGui::PopID();
                    ImGui::Separator();
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::Text("Global Fail Frequency:");
            auto stats = GetGlobalErrorStats(totalQuestions);
            int maxE = 0;
            for (auto& s : stats) if (s.count > maxE) maxE = s.count;
            if (ImGui::BeginChild("Stats", ImVec2(0, 150))) {
                for (auto& s : stats) {
                    float r = (maxE > 0) ? (float)s.count / maxE : 0.0f;
                    ImGui::Text("Q%d", s.id); ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::ProgressBar(r, ImVec2(-1, 20), (std::to_string(s.count) + " errors").c_str());
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();

            if (ImGui::Button("Restart", ImVec2(180, 50))) {
                for (auto& s : soundsA) s.stop(); for (auto& s : soundsB) s.stop();
                currentQuestion = 0; playerChoices.assign(totalQuestions, 0);
                std::shuffle(quiz.begin(), quiz.end(), rng);
                loadingIndex = 0; state = AppState::Loading;
            }
        }
        ImGui::End();
        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();
    return 0;
}