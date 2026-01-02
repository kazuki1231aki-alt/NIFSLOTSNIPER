// ============================================================
// NIF Slot Sniper - Final Stable Version
// ============================================================

#pragma warning(disable: 26454) 

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <map> 
#include <cctype> 
#include <fstream> 
#include <set> 

// 外部プロセス起動用
#ifdef _WIN32
#define NOMINMAX
#undef APIENTRY
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h> 
#endif

namespace fs = std::filesystem;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <NifFile.hpp>
#include <Skin.hpp> 

// ============================================================
// シェーダー
// ============================================================
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 Normal;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    Normal = mat3(transpose(inverse(model))) * aNormal; 
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 Normal;
uniform vec3 lightDir;
uniform vec3 objectColor;
void main() {
    vec3 norm = normalize(Normal);
    if (!gl_FrontFacing) norm = -norm; 
    vec3 light = normalize(lightDir);
    float diff = max(dot(norm, light), 0.4);
    FragColor = vec4(objectColor * diff, 1.0);
}
)";

// ============================================================
// データ構造
// ============================================================
struct RenderMesh {
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    size_t indexCount = 0;
    glm::vec3 color = glm::vec3(1.0f);
    std::string name;
    std::string slotInfo;
    int shapeIndex = -1;
};

struct SlotRecord {
    std::string sourceFile;
    std::string formID;
    std::string editorID;
    std::string type;
    std::string nifPath;
    std::string slotData;
    std::string displayText;
    int id = 0;
};

enum class LogType { Info, Success, Warning, Error };
struct LogEntry {
    std::string message;
    LogType type;
};

// ============================================================
// グローバル変数
// ============================================================
const std::string CONFIG_FILENAME = "config.ini";
const std::string SLOTS_FILENAME = "slots.txt";
const std::string KEYWORDS_FILENAME = "keywords.txt";

std::string g_StatusMessage = "Ready";

// ログ管理
std::vector<LogEntry> g_LogHistory;
bool g_LogAutoScroll = true;
bool g_LogScrollToBottom = false;

// パス設定
char g_InputRootPath[1024] = "";
char g_OutputRootPath[1024] = "";
char g_SlotDataPath[1024] = "";
char g_SynthesisPath[1024] = "";

// 辞書・キーワードリスト
std::map<int, std::string> g_SlotNameMap;
std::vector<std::string> g_KeywordList;

// KID Generator用変数
char g_KIDTargetBuffer[4096] = "";
char g_KIDResultBuffer[4096] = "";
int g_SelectedKeywordIndex = -1;
ImGuiTextFilter g_KIDKeywordFilter;

// メイン装備データ
fs::path g_CurrentNifPath = "";
nifly::NifFile g_NifData;
std::vector<RenderMesh> g_RenderMeshes;
int g_SelectedMeshIndex = -1;

// ペア装備データ
fs::path g_PairedNifPath = "";
nifly::NifFile g_PairedNifData;
bool g_HasPairedFile = false;

// リファレンスデータ
fs::path g_RefNifPath = "";
nifly::NifFile g_RefNifData;
std::vector<RenderMesh> g_RefRenderMeshes;
bool g_ShowRef = true;

// データベース関連
std::map<std::string, std::map<std::string, std::vector<SlotRecord>>> g_SlotDatabase;
std::vector<SlotRecord> g_AllRecords;
ImGuiTextFilter g_SlotFilter;
int g_SelectedRecordID = -1;
std::string g_PreviewSlotStr = "";

// バッチ処理用変数
bool g_ShowBatchPopup = false;
std::string g_BatchTargetESP = "";
int g_BatchCount = 0;

unsigned int g_ShaderProgram = 0;
glm::vec3 g_BodyCenter = glm::vec3(0.0f);

char g_InputBuffer[1024] = "";

// カメラ・マウス
float g_CamDistance = 100.0f;
glm::vec3 g_CamOffset = glm::vec3(0.0f);
float g_ModelRotation[3] = { 0.0f, 0.0f, 0.0f };
double g_LastMouseX = 0.0;
double g_LastMouseY = 0.0;
bool g_MouseInitialized = false;

// ============================================================
// ヘルパー・共通関数
// ============================================================

// ログ出力
void AddLog(const std::string& msg, LogType type = LogType::Info) {
    g_LogHistory.push_back({ msg, type });
    if (g_LogAutoScroll) g_LogScrollToBottom = true;

    g_StatusMessage = msg;
    std::cout << "[LOG] " << msg << std::endl;
}

// ファイルダイアログ
std::string OpenFileDialog(const char* filter = "NIF Files\0*.nif\0All Files\0*.*\0") {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) return std::string(szFile);
#endif
    return "";
}

std::string SaveFileDialog(const std::string& defaultName, const std::string& initialDir = "") {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };
    strncpy_s(szFile, defaultName.c_str(), _TRUNCATE);
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "NIF Files\0*.nif\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!initialDir.empty()) ofn.lpstrInitialDir = initialDir.c_str();
    if (GetSaveFileNameA(&ofn) == TRUE) return std::string(szFile);
#endif
    return "";
}

// スロット名管理
void InitDefaultSlots() {
    g_SlotNameMap.clear();
    g_SlotNameMap[30] = "Head"; g_SlotNameMap[31] = "Hair"; g_SlotNameMap[32] = "Body";
    g_SlotNameMap[33] = "Hands"; g_SlotNameMap[34] = "Forearms"; g_SlotNameMap[35] = "Amulet";
    g_SlotNameMap[36] = "Ring"; g_SlotNameMap[37] = "Feet"; g_SlotNameMap[38] = "Calves";
    g_SlotNameMap[39] = "Shield"; g_SlotNameMap[40] = "Tail"; g_SlotNameMap[41] = "LongHair";
    g_SlotNameMap[42] = "Circlet"; g_SlotNameMap[43] = "Ears"; g_SlotNameMap[44] = "Face/Mouth";
    g_SlotNameMap[45] = "Neck"; g_SlotNameMap[50] = "Decapitated Head"; g_SlotNameMap[51] = "Decapitated";
    g_SlotNameMap[60] = "Misc"; g_SlotNameMap[61] = "FX01";
}

std::string GetSlotName(int slotID) {
    if (g_SlotNameMap.count(slotID)) return g_SlotNameMap.at(slotID);
    return "Unnamed";
}

// 設定ファイル管理
void LoadUnifiedConfig() {
    InitDefaultSlots();
    g_KeywordList.clear();
    g_KeywordList = { "SexLabNoStrip", "OStimNoStrip", "SOS_Revealing", "ArmorCuirass" };

    std::ifstream file(CONFIG_FILENAME);
    if (!file.is_open()) return;

    std::string line;
    int currentSection = 0;
    while (std::getline(file, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        if (line.empty() || line[0] == ';') continue;

        if (line == "[General]") { currentSection = 1; continue; }
        if (line == "[Slots]") { currentSection = 2; g_SlotNameMap.clear(); continue; }
        if (line == "[Keywords]") { currentSection = 3; g_KeywordList.clear(); continue; }

        if (currentSection == 1) {
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                if (key == "InputRoot") strncpy_s(g_InputRootPath, value.c_str(), _TRUNCATE);
                else if (key == "OutputRoot") strncpy_s(g_OutputRootPath, value.c_str(), _TRUNCATE);
                else if (key == "SlotDataPath") strncpy_s(g_SlotDataPath, value.c_str(), _TRUNCATE);
                else if (key == "SynthesisPath") strncpy_s(g_SynthesisPath, value.c_str(), _TRUNCATE);
            }
        }
        else if (currentSection == 2) {
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                try {
                    int id = std::stoi(line.substr(0, delimiterPos));
                    std::string name = line.substr(delimiterPos + 1);
                    g_SlotNameMap[id] = name;
                }
                catch (...) {}
            }
        }
        else if (currentSection == 3) {
            g_KeywordList.push_back(line);
        }
    }
    AddLog("Unified Config Loaded.");
}

void SaveUnifiedConfig() {
    std::ofstream file(CONFIG_FILENAME);
    if (file.is_open()) {
        file << "[General]\n";
        file << "InputRoot=" << g_InputRootPath << "\n";
        file << "OutputRoot=" << g_OutputRootPath << "\n";
        file << "SlotDataPath=" << g_SlotDataPath << "\n";
        file << "SynthesisPath=" << g_SynthesisPath << "\n\n";
        file << "[Slots]\n";
        for (const auto& pair : g_SlotNameMap) file << pair.first << "=" << pair.second << "\n";
        file << "\n[Keywords]\n";
        for (const auto& kw : g_KeywordList) file << kw << "\n";
        AddLog("Unified Config Saved.", LogType::Success);
    }
    else {
        AddLog("Failed to save config.", LogType::Error);
    }
}

// その他ヘルパー
glm::vec3 GetColorFromIndex(int index) {
    static const glm::vec3 palette[] = {
        {1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f}, {0.5f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f},
        {0.5f, 1.0f, 1.0f}, {1.0f, 0.5f, 1.0f}, {1.0f, 0.7f, 0.2f}, {0.2f, 0.8f, 0.8f}
    };
    return palette[index % 8];
}

std::string FormatSlotStringWithNames(const std::string& rawSlots) {
    std::stringstream ss(rawSlots);
    std::string segment;
    std::string result;
    bool first = true;
    while (std::getline(ss, segment, ',')) {
        try {
            int id = std::stoi(segment);
            if (!first) result += ", ";
            result += std::to_string(id) + " (" + GetSlotName(id) + ")";
            first = false;
        }
        catch (...) {}
    }
    return result;
}

std::vector<int> ParseSlotString(const std::string& slotStr) {
    std::vector<int> slots;
    std::string slotPart = slotStr;
    size_t lastSemi = slotStr.rfind(';');
    if (lastSemi != std::string::npos) slotPart = slotStr.substr(lastSemi + 1);
    std::stringstream ss(slotPart);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        try { slots.push_back(std::stoi(segment)); }
        catch (...) {}
    }
    return slots;
}

unsigned int CreateShader(const char* vSource, const char* fSource) {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

std::string GetRelativeMeshesPath(const fs::path& fullPath) {
    std::string pathStr = fullPath.string();
    std::string lowerPath = pathStr;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    size_t pos = lowerPath.find("meshes");
    if (pos != std::string::npos) return pathStr.substr(pos);
    return pathStr;
}

std::string GetPairedFilename(const std::string& path) {
    fs::path p(path);
    std::string stem = p.stem().string();
    std::string ext = p.extension().string();
    std::string parent = p.parent_path().string();
    if (stem.size() < 2) return "";
    std::string suffix = stem.substr(stem.size() - 2);
    if (suffix == "_0") {
        stem.replace(stem.size() - 2, 2, "_1");
        return (fs::path(parent) / (stem + ext)).string();
    }
    if (suffix == "_1") {
        stem.replace(stem.size() - 2, 2, "_0");
        return (fs::path(parent) / (stem + ext)).string();
    }
    return "";
}

std::string CreateDefaultSaveName(const std::string& originalPath) {
    fs::path p(originalPath);
    std::string stem = p.stem().string();
    if (stem.size() >= 2) {
        std::string suffix = stem.substr(stem.size() - 2);
        if (suffix == "_0") return stem.substr(0, stem.size() - 2) + "_out_0.nif";
        if (suffix == "_1") return stem.substr(0, stem.size() - 2) + "_out_1.nif";
    }
    return stem + "_out.nif";
}

std::vector<std::string> SplitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string NormalizePathForCompare(const std::string& path) {
    std::string res = path;
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    std::replace(res.begin(), res.end(), '\\', '/');
    return res;
}

// ============================================================
// コアロジック (関数定義)
// ============================================================

void UpdateMeshList(nifly::NifFile& targetNif, std::vector<RenderMesh>& outMeshes, bool isRef) {
    for (auto& mesh : outMeshes) {
        if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
        if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
        if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    }
    outMeshes.clear();
    if (!isRef) g_SelectedMeshIndex = -1;

    glm::vec3 minBounds(99999.0f);
    glm::vec3 maxBounds(-99999.0f);
    bool hasVertices = false;

    auto shapes = targetNif.GetShapes();

    for (size_t k = 0; k < shapes.size(); ++k) {
        auto shape = shapes[k];
        if (!shape) continue;

        std::vector<nifly::Vector3> rawVerts;
        std::vector<nifly::Triangle> rawTris;
        std::string currentSlots = "None";

        if (auto bsShape = dynamic_cast<nifly::BSTriShape*>(shape)) {
            if (bsShape->triangles.empty()) continue;
            rawTris = bsShape->triangles;
            for (const auto& v : bsShape->vertData) {
                rawVerts.push_back({ v.vert.x, v.vert.y, v.vert.z });
            }

            auto skinRef = bsShape->SkinInstanceRef();
            if (!skinRef->IsEmpty()) {
                auto skinObj = targetNif.GetHeader().GetBlock<nifly::NiObject>(skinRef->index);
                if (skinObj) {
                    if (auto dismemberSkin = dynamic_cast<nifly::BSDismemberSkinInstance*>(skinObj)) {
                        std::stringstream ss;
                        for (size_t i = 0; i < dismemberSkin->partitions.size(); ++i) {
                            if (i > 0) ss << ", ";
                            int slotID = dismemberSkin->partitions[i].partID;
                            std::string name = GetSlotName(slotID);
                            ss << slotID << " (" << name << ")";
                        }
                        currentSlots = ss.str();
                    }
                    else {
                        currentSlots = "NiSkin";
                    }
                }
            }
        }
        else {
            continue;
        }

        if (rawVerts.empty()) continue;

        if (!isRef) {
            for (const auto& v : rawVerts) {
                minBounds = glm::min(minBounds, glm::vec3(v.x, v.y, v.z));
                maxBounds = glm::max(maxBounds, glm::vec3(v.x, v.y, v.z));
            }
            hasVertices = true;
        }

        struct Vertex { float x, y, z; float nx, ny, nz; };
        std::vector<Vertex> gpuVerts;
        gpuVerts.reserve(rawVerts.size());
        for (size_t i = 0; i < rawVerts.size(); ++i) {
            gpuVerts.push_back({ rawVerts[i].x, rawVerts[i].y, rawVerts[i].z, 0.0f, 0.0f, 1.0f });
        }

        std::vector<unsigned int> indices;
        indices.reserve(rawTris.size() * 3);
        for (const auto& t : rawTris) {
            indices.push_back(t.p1); indices.push_back(t.p2); indices.push_back(t.p3);
        }

        RenderMesh mesh;
        mesh.indexCount = indices.size();
        if (isRef) {
            mesh.color = glm::vec3(0.5f, 0.5f, 0.5f);
        }
        else {
            mesh.color = GetColorFromIndex((int)outMeshes.size());
        }
        if (!shape->name.get().empty()) mesh.name = shape->name.get();
        else mesh.name = "Mesh " + std::to_string(outMeshes.size());
        mesh.slotInfo = currentSlots;
        mesh.shapeIndex = (int)k;

        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);

        glBindVertexArray(mesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
        glBufferData(GL_ARRAY_BUFFER, gpuVerts.size() * sizeof(Vertex), gpuVerts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        outMeshes.push_back(mesh);
    }

    if (!isRef && hasVertices) {
        g_BodyCenter = (minBounds + maxBounds) * 0.5f;
    }
}

void LoadNifFileCore(const std::string& path) {
    if (g_NifData.Load(path) == 0) {
        g_CurrentNifPath = path;
        g_HasPairedFile = false;

        UpdateMeshList(g_NifData, g_RenderMeshes, false);
        AddLog("Loaded: " + GetRelativeMeshesPath(g_CurrentNifPath));

        std::string pairName = GetPairedFilename(g_CurrentNifPath.string());
        if (!pairName.empty() && fs::exists(pairName)) {
            if (g_PairedNifData.Load(pairName) == 0) {
                g_HasPairedFile = true;
                g_PairedNifPath = pairName;
                AddLog(" + Pair Loaded: " + GetRelativeMeshesPath(g_PairedNifPath));
            }
        }
    }
    else {
        AddLog("Load Failed: " + path, LogType::Error);
    }
}

void LoadSlotDataFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    g_SlotDatabase.clear();
    g_AllRecords.clear();

    std::string line;
    int idCounter = 0;

    while (std::getline(file, line)) {
        auto cols = SplitString(line, ';');
        if (cols.size() >= 6) {
            if (cols[4].empty()) continue;

            std::string typeStr = cols[3];
            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
            if (typeStr.find("ARMO") == std::string::npos) {
                continue;
            }

            SlotRecord rec;
            rec.sourceFile = cols[0];
            rec.formID = cols[1];
            rec.editorID = cols[2];
            rec.type = cols[3];
            rec.nifPath = cols[4];
            rec.slotData = cols[5];

            fs::path p(rec.nifPath);
            rec.displayText = rec.editorID + " [" + rec.slotData + "] (" + p.filename().string() + ")";
            rec.id = ++idCounter;

            g_SlotDatabase[rec.sourceFile][rec.nifPath].push_back(rec);
            g_AllRecords.push_back(rec);
        }
    }
    g_SelectedRecordID = -1;
    AddLog("Loaded " + std::to_string(g_AllRecords.size()) + " records from " + path, LogType::Success);
}

void AutoMatchRecord() {
    if (g_AllRecords.empty()) { AddLog("Slot data empty.", LogType::Warning); return; }
    if (g_CurrentNifPath.empty()) { AddLog("No NIF loaded.", LogType::Warning); return; }

    std::string currentRel = GetRelativeMeshesPath(g_CurrentNifPath);
    std::string searchKey = NormalizePathForCompare(currentRel);

    const SlotRecord* foundRec = nullptr;
    for (const auto& rec : g_AllRecords) {
        std::string recordPath = NormalizePathForCompare(rec.nifPath);
        if (searchKey.find(recordPath) != std::string::npos || recordPath.find(searchKey) != std::string::npos) {
            foundRec = &rec;
            break;
        }
    }

    if (foundRec) {
        g_SelectedRecordID = foundRec->id;
        g_PreviewSlotStr = foundRec->slotData;
        memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
        AddLog("Match Found: " + foundRec->editorID, LogType::Success);
    }
    else {
        AddLog("No match found in database.", LogType::Warning);
    }
}

void ApplySlotChanges(int meshIndex, const std::string& slotStr) {
    if (meshIndex < 0 || meshIndex >= g_RenderMeshes.size()) return;
    if (slotStr.empty()) return;

    std::vector<int> newSlots = ParseSlotString(slotStr);
    if (newSlots.empty()) return;

    int shapeIdx = g_RenderMeshes[meshIndex].shapeIndex;
    std::string targetMeshName = g_RenderMeshes[meshIndex].name;

    auto ApplyToShape = [&](nifly::NifFile& nifData, int sIdx) {
        auto shapes = nifData.GetShapes();
        if (sIdx >= shapes.size()) return false;
        auto bsShape = dynamic_cast<nifly::BSTriShape*>(shapes[sIdx]);
        if (!bsShape) return false;
        auto skinRef = bsShape->SkinInstanceRef();
        if (!skinRef->IsEmpty()) {
            auto skinObj = nifData.GetHeader().GetBlock<nifly::NiObject>(skinRef->index);
            if (auto dismemberSkin = dynamic_cast<nifly::BSDismemberSkinInstance*>(skinObj)) {
                for (size_t i = 0; i < dismemberSkin->partitions.size(); ++i) {
                    if (i < newSlots.size()) dismemberSkin->partitions[i].partID = (uint16_t)newSlots[i];
                }
                return true;
            }
        }
        return false;
        };

    bool mainApplied = ApplyToShape(g_NifData, shapeIdx);
    bool pairApplied = false;
    if (g_HasPairedFile) {
        auto pairShapes = g_PairedNifData.GetShapes();
        for (size_t k = 0; k < pairShapes.size(); ++k) {
            auto shape = pairShapes[k];
            if (shape && shape->name.get() == targetMeshName) {
                pairApplied = ApplyToShape(g_PairedNifData, (int)k);
                break;
            }
        }
    }

    if (mainApplied) {
        AddLog("Applied Slot Changes" + std::string(pairApplied ? " (Synced)" : ""), LogType::Success);
        UpdateMeshList(g_NifData, g_RenderMeshes, false);
    }
    else {
        AddLog("Failed to apply change (Not a partition mesh?)", LogType::Error);
    }
    g_SelectedMeshIndex = meshIndex;
}

void RunBatchExport() {
    if (strlen(g_InputRootPath) == 0 || strlen(g_OutputRootPath) == 0) {
        AddLog("Error: Input and Output roots must be set.", LogType::Error);
        return;
    }

    std::vector<SlotRecord> targets;
    if (g_BatchTargetESP.empty()) {
        targets = g_AllRecords;
    }
    else {
        if (g_SlotDatabase.count(g_BatchTargetESP)) {
            for (auto& [nifPath, recs] : g_SlotDatabase[g_BatchTargetESP]) {
                if (!recs.empty()) targets.push_back(recs[0]);
            }
        }
    }

    if (targets.empty()) {
        AddLog("No records found to export.", LogType::Warning);
        return;
    }

    AddLog("Batch Export Started. Targets: " + std::to_string(targets.size()), LogType::Info);

    int successCount = 0;
    int errorCount = 0;
    std::set<std::string> processedFiles;

    auto MakeOutputPath = [&](std::string rawPath) -> fs::path {
        while (!rawPath.empty() && (rawPath.front() == '/' || rawPath.front() == '\\')) {
            rawPath.erase(0, 1);
        }
        std::string lower = rawPath;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        bool startsWithMeshes = false;
        if (lower.rfind("meshes", 0) == 0) {
            if (lower.length() == 6) startsWithMeshes = true;
            else if (lower[6] == '/' || lower[6] == '\\') startsWithMeshes = true;
        }

        if (startsWithMeshes) return fs::path(rawPath);
        else return fs::path("meshes") / rawPath;
        };

    for (const auto& rec : targets) {
        std::string normPath = NormalizePathForCompare(rec.nifPath);
        if (processedFiles.count(normPath)) continue;
        processedFiles.insert(normPath);

        fs::path inFullPath = fs::path(g_InputRootPath) / rec.nifPath;
        fs::path outFullPath = fs::path(g_OutputRootPath) / MakeOutputPath(rec.nifPath);

        if (!fs::exists(inFullPath)) {
            AddLog("[Error] Input file not found: " + rec.nifPath, LogType::Error);
            errorCount++;
            continue;
        }

        try { fs::create_directories(outFullPath.parent_path()); }
        catch (...) {
            AddLog("[Error] Failed to create directory: " + outFullPath.parent_path().string(), LogType::Error);
            errorCount++;
            continue;
        }

        auto ProcessSingleFile = [&](const fs::path& inP, const fs::path& outP, const std::string& sData) {
            nifly::NifFile tempNif;
            if (tempNif.Load(inP.string()) == 0) {
                std::vector<int> slots = ParseSlotString(sData);
                if (!slots.empty()) {
                    auto shapes = tempNif.GetShapes();
                    for (size_t i = 0; i < shapes.size(); ++i) {
                        auto bsShape = dynamic_cast<nifly::BSTriShape*>(shapes[i]);
                        if (!bsShape) continue;
                        auto skinRef = bsShape->SkinInstanceRef();
                        if (!skinRef->IsEmpty()) {
                            auto skinObj = tempNif.GetHeader().GetBlock<nifly::NiObject>(skinRef->index);
                            if (auto dismember = dynamic_cast<nifly::BSDismemberSkinInstance*>(skinObj)) {
                                for (size_t k = 0; k < dismember->partitions.size(); ++k) {
                                    if (k < slots.size()) dismember->partitions[k].partID = (uint16_t)slots[k];
                                }
                            }
                        }
                    }
                }
                if (tempNif.Save(outP.string()) == 0) return true;
            }
            return false;
            };

        if (ProcessSingleFile(inFullPath, outFullPath, rec.slotData)) {
            successCount++;
            AddLog("Exported: " + rec.nifPath, LogType::Success);

            std::string pairName = GetPairedFilename(rec.nifPath);
            if (!pairName.empty()) {
                fs::path pairIn = fs::path(g_InputRootPath) / pairName;
                fs::path pairOut = fs::path(g_OutputRootPath) / MakeOutputPath(pairName);

                if (fs::exists(pairIn)) {
                    if (ProcessSingleFile(pairIn, pairOut, rec.slotData)) {
                        AddLog(" + Paired File Exported: " + pairName, LogType::Success);
                    }
                    else {
                        AddLog("[Warning] Failed to process paired file: " + pairName, LogType::Warning);
                    }
                }
            }
        }
        else {
            AddLog("[Error] Failed to load/save NIF: " + rec.nifPath, LogType::Error);
            errorCount++;
        }
    }

    AddLog("Batch Finished. Success: " + std::to_string(successCount) + ", Errors: " + std::to_string(errorCount), LogType::Info);
}

void DrawKIDWindow() {
    ImGui::SetNextWindowPos(ImVec2(780, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("KID Generator");

    ImGui::Text("1. Select Keyword");
    g_KIDKeywordFilter.Draw("Filter##KID");

    std::vector<std::string> filteredKeywords;
    for (const auto& k : g_KeywordList) {
        if (g_KIDKeywordFilter.PassFilter(k.c_str())) filteredKeywords.push_back(k);
    }

    if (ImGui::BeginCombo("Keyword", (g_SelectedKeywordIndex >= 0 && g_SelectedKeywordIndex < filteredKeywords.size()) ? filteredKeywords[g_SelectedKeywordIndex].c_str() : "Select...")) {
        for (int i = 0; i < filteredKeywords.size(); i++) {
            bool isSelected = (g_SelectedKeywordIndex == i);
            if (ImGui::Selectable(filteredKeywords[i].c_str(), isSelected)) g_SelectedKeywordIndex = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::Text("2. Add Targets (from Slot DB)");

    std::string currentDBSelect = "(None)";
    if (g_SelectedRecordID != -1) {
        for (const auto& rec : g_AllRecords) {
            if (rec.id == g_SelectedRecordID) {
                currentDBSelect = rec.editorID;
                break;
            }
        }
    }
    ImGui::TextDisabled("Selected in DB: %s", currentDBSelect.c_str());

    if (ImGui::Button("Add Selected EditorID")) {
        if (currentDBSelect != "(None)") {
            std::string currentBuffer = g_KIDTargetBuffer;
            if (!currentBuffer.empty()) currentBuffer += ",";
            currentBuffer += currentDBSelect;
            strncpy_s(g_KIDTargetBuffer, currentBuffer.c_str(), _TRUNCATE);
        }
        else {
            AddLog("No item selected in Slot Database.", LogType::Warning);
        }
    }

    ImGui::InputTextMultiline("Target List", g_KIDTargetBuffer, sizeof(g_KIDTargetBuffer), ImVec2(-1, 60));

    ImGui::Separator();
    ImGui::Text("3. Generate Config");

    if (ImGui::Button("Generate KID String")) {
        if (g_SelectedKeywordIndex >= 0 && g_SelectedKeywordIndex < filteredKeywords.size()) {
            std::string kw = filteredKeywords[g_SelectedKeywordIndex];
            std::string targets = g_KIDTargetBuffer;
            std::string result = "Keyword = " + kw + "|Armor|" + targets;
            strncpy_s(g_KIDResultBuffer, result.c_str(), _TRUNCATE);
        }
        else {
            AddLog("Please select a Keyword first.", LogType::Warning);
        }
    }

    ImGui::InputTextMultiline("Result", g_KIDResultBuffer, sizeof(g_KIDResultBuffer), ImVec2(-1, 60));

    if (ImGui::Button("Append to output_kid.ini")) {
        if (strlen(g_KIDResultBuffer) > 0) {
            std::ofstream outfile("output_kid.ini", std::ios::app);
            if (outfile.is_open()) {
                outfile << g_KIDResultBuffer << "\n";
                outfile.close();
                AddLog("Appended to output_kid.ini", LogType::Success);
            }
            else {
                AddLog("Failed to write to file.", LogType::Error);
            }
        }
    }

    ImGui::End();
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// ============================================================
// メイン関数
// ============================================================
int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "NIF Slot Sniper 3D", nullptr, nullptr);
    if (window == nullptr) return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return 1;

    g_ShaderProgram = CreateShader(vertexShaderSource, fragmentShaderSource);
    glDisable(GL_CULL_FACE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glEnable(GL_DEPTH_TEST);
    ImVec4 clear_color = ImVec4(0.2f, 0.2f, 0.25f, 1.00f);

    LoadUnifiedConfig();

    if (strlen(g_SlotDataPath) > 0) {
        LoadSlotDataFile(g_SlotDataPath);
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // --- マウスドラッグ (回転・パン) ---
        if (!ImGui::GetIO().WantCaptureMouse) {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            if (!g_MouseInitialized) {
                g_LastMouseX = mouseX; g_LastMouseY = mouseY; g_MouseInitialized = true;
            }
            float dx = (float)(mouseX - g_LastMouseX);
            float dy = (float)(mouseY - g_LastMouseY);
            g_LastMouseX = mouseX; g_LastMouseY = mouseY;

            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                    float panSpeed = g_CamDistance * 0.001f;
                    g_CamOffset.x += dx * panSpeed; g_CamOffset.y -= dy * panSpeed;
                }
                else {
                    g_ModelRotation[1] += dx * 0.5f; g_ModelRotation[0] += dy * 0.5f;
                }
            }
        }
        else {
            glfwGetCursorPos(window, &g_LastMouseX, &g_LastMouseY);
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            g_CamOffset = glm::vec3(0.0f);
            g_ModelRotation[0] = 0.0f; g_ModelRotation[1] = 0.0f; g_ModelRotation[2] = 0.0f;
            g_CamDistance = 100.0f;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- メインコントロール ---
        {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(350, 600), ImGuiCond_FirstUseEver);
            ImGui::Begin("Control Panel (Single NIF)");

            if (ImGui::Button("Load Target NIF")) {
                std::string selectedFile = OpenFileDialog();
                if (!selectedFile.empty()) LoadNifFileCore(selectedFile);
            }
            ImGui::SameLine();

            if (ImGui::Button("Save This NIF As...")) {
                if (!g_CurrentNifPath.empty()) {
                    std::string defaultName = CreateDefaultSaveName(g_CurrentNifPath.string());
                    std::string initialDir = "";
                    if (strlen(g_OutputRootPath) > 0) {
                        std::string relPath = GetRelativeMeshesPath(g_CurrentNifPath);
                        fs::path fullOutPath = fs::path(g_OutputRootPath) / fs::path(relPath).parent_path();
                        try { fs::create_directories(fullOutPath); initialDir = fullOutPath.string(); }
                        catch (...) {}
                    }
                    std::string savePath = SaveFileDialog(defaultName, initialDir);
                    if (!savePath.empty()) {
                        if (g_NifData.Save(savePath) == 0) {
                            AddLog("Saved Main: " + fs::path(savePath).filename().string(), LogType::Success);
                            if (g_HasPairedFile) {
                                std::string pairSavePath = GetPairedFilename(savePath);
                                if (!pairSavePath.empty()) {
                                    if (g_PairedNifData.Save(pairSavePath) == 0) {
                                        AddLog(" + Saved Pair: " + fs::path(pairSavePath).filename().string(), LogType::Success);
                                    }
                                }
                            }
                        }
                        else {
                            AddLog("Save Failed!", LogType::Error);
                        }
                    }
                }
            }

            if (!g_CurrentNifPath.empty()) {
                std::string relPath = GetRelativeMeshesPath(g_CurrentNifPath);
                fs::path p(relPath);
                ImGui::Text("Path: %s", p.parent_path().string().c_str());
                ImGui::Text("File: %s", p.filename().string().c_str());
            }
            else {
                ImGui::Text("File: (None)");
            }

            if (ImGui::Button("Auto Match by Path")) {
                AutoMatchRecord();
            }

            // ステータス表示
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", g_StatusMessage.c_str());

            ImGui::Separator();

            if (ImGui::Button("Load Ref Body")) {
                std::string selectedFile = OpenFileDialog();
                if (!selectedFile.empty()) {
                    g_RefNifPath = selectedFile;
                    if (g_RefNifData.Load(g_RefNifPath.string()) == 0) {
                        UpdateMeshList(g_RefNifData, g_RefRenderMeshes, true);
                    }
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("Show Ref", &g_ShowRef);

            if (!g_RefNifPath.empty()) {
                ImGui::Text("Ref: %s", fs::path(g_RefNifPath).filename().string().c_str());
            }

            ImGui::Separator();

            ImGui::DragFloat("Zoom", &g_CamDistance, -1.0f, 5.0f, 500.0f, "Dist: %.1f");
            ImGui::SliderFloat3("Rotation", g_ModelRotation, -180.0f, 180.0f);

            ImGui::Separator();
            ImGui::Text("Mesh List (Select to Edit):");

            ImGui::BeginChild("MeshList", ImVec2(0, 150), true);
            for (int i = 0; i < g_RenderMeshes.size(); ++i) {
                const auto& mesh = g_RenderMeshes[i];
                bool isSelected = (g_SelectedMeshIndex == i);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(mesh.color.r, mesh.color.g, mesh.color.b, 1.0f));

                std::string label = "* " + mesh.name + " [" + mesh.slotInfo + "]";

                if (isSelected) {
                    std::string pending = "";
                    if (strlen(g_InputBuffer) > 0) pending = g_InputBuffer;
                    else if (!g_PreviewSlotStr.empty()) pending = g_PreviewSlotStr;

                    if (!pending.empty()) {
                        label += " -> [" + FormatSlotStringWithNames(pending) + "]";
                    }
                }

                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    g_SelectedMeshIndex = i;
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();

            ImGui::Separator();

            if (g_SelectedMeshIndex != -1) {
                ImGui::Text("Edit Slots for: %s", g_RenderMeshes[g_SelectedMeshIndex].name.c_str());

                if (ImGui::InputText("Manual Input", g_InputBuffer, sizeof(g_InputBuffer))) {
                    g_PreviewSlotStr.clear();
                }

                std::string targetSlots = "";
                if (strlen(g_InputBuffer) > 0) targetSlots = g_InputBuffer;
                else targetSlots = g_PreviewSlotStr;

                if (ImGui::Button("Apply Change")) {
                    ApplySlotChanges(g_SelectedMeshIndex, targetSlots);
                }
            }
            else {
                ImGui::TextDisabled("(Select a mesh to edit slots)");
            }

            ImGui::End();
        }

        // --- データベースウィンドウ ---
        {
            ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
            ImGui::Begin("Slot Database");

            if (ImGui::Button("Load slotdata.txt")) {
                std::string file = OpenFileDialog("Text Files\0*.txt\0All Files\0*.*\0");
                if (!file.empty()) {
                    LoadSlotDataFile(file);
                    strncpy_s(g_SlotDataPath, file.c_str(), _TRUNCATE);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Export ALL NIFs")) {
                if (g_AllRecords.empty()) AddLog("No records loaded.", LogType::Warning);
                else if (strlen(g_InputRootPath) == 0 || strlen(g_OutputRootPath) == 0) AddLog("Set Input/Output roots first!", LogType::Error);
                else {
                    g_BatchTargetESP = ""; // 全件
                    g_BatchCount = (int)g_AllRecords.size();
                    g_ShowBatchPopup = true;
                }
            }

            ImGui::Separator();
            ImGui::Text("IO Settings:");
            ImGui::InputText("Input Root (Read)", g_InputRootPath, sizeof(g_InputRootPath));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Example: D:\\Skyrim\\Data (Base for 'meshes' path)");

            ImGui::InputText("Output Root (Save)", g_OutputRootPath, sizeof(g_OutputRootPath));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Example: D:\\Mods\\Overwrite (Base for saving)");

            // Synthesis 連携
            ImGui::Text("Integration:");
            ImGui::InputText("Synthesis Path", g_SynthesisPath, sizeof(g_SynthesisPath));
            if (ImGui::Button("Launch Synthesis")) {
                if (strlen(g_SynthesisPath) > 0) {
                    ShellExecuteA(NULL, "open", g_SynthesisPath, NULL, NULL, SW_SHOWNORMAL);
                    AddLog("Launching Synthesis...", LogType::Success);
                }
                else {
                    AddLog("Please set Synthesis Path first.", LogType::Error);
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tip: Update ESP records using Synthesis.");
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Note: Use Synthesis Patcher for ESP changes.");

            if (ImGui::Button("Save All Settings")) {
                SaveUnifiedConfig();
            }

            ImGui::Separator();

            ImGui::Text("%d records in %d ESPs", (int)g_AllRecords.size(), (int)g_SlotDatabase.size());
            g_SlotFilter.Draw("Filter");

            ImGui::Separator();

            ImGui::BeginChild("RecordList", ImVec2(0, 0), true);

            auto DrawRecordItem = [&](const SlotRecord& rec) {
                bool isSelected = (g_SelectedRecordID == rec.id);
                if (ImGui::Selectable(rec.displayText.c_str(), isSelected)) {
                    g_SelectedRecordID = rec.id;
                    g_PreviewSlotStr = rec.slotData;
                    memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
                }
                if (isSelected) ImGui::SetItemDefaultFocus();

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (strlen(g_InputRootPath) > 0) {
                        fs::path fullPath = fs::path(g_InputRootPath) / rec.nifPath;
                        LoadNifFileCore(fullPath.string());
                    }
                    else {
                        AddLog("Please set Input Root first!", LogType::Warning);
                    }
                }
                };

            if (g_SlotFilter.IsActive()) {
                for (const auto& rec : g_AllRecords) {
                    if (g_SlotFilter.PassFilter(rec.displayText.c_str())) DrawRecordItem(rec);
                }
            }
            else {
                for (auto& [espName, nifMap] : g_SlotDatabase) {
                    bool treeOpen = ImGui::TreeNode(espName.c_str());

                    ImGui::SameLine();
                    std::string btnLabel = "Export##" + espName;
                    if (ImGui::SmallButton(btnLabel.c_str())) {
                        if (strlen(g_InputRootPath) == 0 || strlen(g_OutputRootPath) == 0) AddLog("Set Input/Output roots first!", LogType::Error);
                        else {
                            g_BatchTargetESP = espName;
                            g_BatchCount = 0;
                            for (auto& [n, r] : nifMap) if (!r.empty()) g_BatchCount++;
                            g_ShowBatchPopup = true;
                        }
                    }

                    if (treeOpen) {
                        for (auto& [nifPath, records] : nifMap) {
                            if (records.empty()) continue;
                            const auto& rep = records[0];

                            if (records.size() == 1) {
                                DrawRecordItem(rep);
                            }
                            else {
                                std::string label = rep.displayText + " (+" + std::to_string(records.size() - 1) + " variants)";
                                bool isSelected = (g_SelectedRecordID == rep.id);
                                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                                bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)rep.id, flags, "%s", label.c_str());

                                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                                    g_SelectedRecordID = rep.id;
                                    g_PreviewSlotStr = rep.slotData;
                                    memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
                                }
                                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                                    if (strlen(g_InputRootPath) > 0) {
                                        fs::path fullPath = fs::path(g_InputRootPath) / rep.nifPath;
                                        LoadNifFileCore(fullPath.string());
                                    }
                                    else {
                                        AddLog("Please set Input Root first!", LogType::Warning);
                                    }
                                }

                                if (isOpen) {
                                    for (size_t i = 1; i < records.size(); ++i) {
                                        DrawRecordItem(records[i]);
                                    }
                                    ImGui::TreePop();
                                }
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            }

            ImGui::EndChild();
            ImGui::End();
        }

        DrawKIDWindow();

        // ログコンソール
        {
            ImGui::SetNextWindowPos(ImVec2(10, 620), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(760, 170), ImGuiCond_FirstUseEver);
            ImGui::Begin("Log Console");

            if (ImGui::Button("Clear")) g_LogHistory.clear();
            ImGui::SameLine();
            ImGui::Checkbox("Auto Scroll", &g_LogAutoScroll);

            ImGui::Separator();
            ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            for (const auto& log : g_LogHistory) {
                ImVec4 color = ImVec4(1, 1, 1, 1);
                if (log.type == LogType::Success) color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                else if (log.type == LogType::Warning) color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                else if (log.type == LogType::Error) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(log.message.c_str());
                ImGui::PopStyleColor();
            }

            if (g_LogScrollToBottom && g_LogAutoScroll) {
                ImGui::SetScrollHereY(1.0f);
                g_LogScrollToBottom = false;
            }

            ImGui::EndChild();
            ImGui::End();
        }

        if (g_ShowBatchPopup) {
            ImGui::OpenPopup("Batch Export Confirmation");
            g_ShowBatchPopup = false;
        }

        if (ImGui::BeginPopupModal("Batch Export Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            std::string targetText = g_BatchTargetESP.empty() ? "ALL Records" : ("ESP: " + g_BatchTargetESP);
            ImGui::Text("Target: %s", targetText.c_str());
            ImGui::Text("Files to Process: Approx %d (excluding pairs)", g_BatchCount);
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Warning: This will overwrite files in the Output Root folder.");
            ImGui::Separator();

            if (ImGui::Button("Execute", ImVec2(120, 0))) {
                RunBatchExport();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(g_ShaderProgram);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)display_w / (float)display_h, 0.1f, 10000.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(g_CamOffset.x, g_CamOffset.y, -g_CamDistance));
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(g_ModelRotation[0]), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(g_ModelRotation[1] + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(g_ModelRotation[2]), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::translate(model, -g_BodyCenter);

        glUniformMatrix4fv(glGetUniformLocation(g_ShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(g_ShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(g_ShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(g_ShaderProgram, "lightDir"), 0.5f, 1.0f, 0.3f);

        if (g_ShowRef) {
            for (const auto& mesh : g_RefRenderMeshes) {
                glUniform3f(glGetUniformLocation(g_ShaderProgram, "objectColor"), mesh.color.r, mesh.color.g, mesh.color.b);
                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indexCount, GL_UNSIGNED_INT, 0);
            }
        }
        for (int i = 0; i < g_RenderMeshes.size(); ++i) {
            const auto& mesh = g_RenderMeshes[i];
            glm::vec3 c = mesh.color;
            if (i == g_SelectedMeshIndex) c = glm::vec3(1.2f, 1.2f, 1.2f);

            glUniform3f(glGetUniformLocation(g_ShaderProgram, "objectColor"), c.r, c.g, c.b);
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    SaveUnifiedConfig();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}