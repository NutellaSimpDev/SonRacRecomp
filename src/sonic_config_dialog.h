/**
 * @file        sonic_config_dialog.h
 * @brief       Interactive Configuration Screen for Sonic & All-Stars Racing Transformed
 *              Provides real-time resolution, graphics settings, connected gamepad detection,
 *              live input testing, and persistence to sonicracingtransformed.toml.
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cmath>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/ui/flags.h>
#include <rex/graphics/flags.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/window.h>
#include <rex/logging.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>

class SonicConfigDialog : public rex::ui::ImGuiDialog {
 public:
  SonicConfigDialog(rex::ui::ImGuiDrawer* imgui_drawer,
                    std::filesystem::path config_path,
                    rex::ui::Window* window = nullptr)
      : rex::ui::ImGuiDialog(imgui_drawer),
        config_path_(std::move(config_path)),
        window_(window) {
    LoadCurrentSettings();
  }

  ~SonicConfigDialog() = default;

  bool should_close() const { return should_close_; }

 protected:
  void OnDraw(ImGuiIO& io) override {
    (void)io;
    ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 80), ImGuiCond_FirstUseEver);

    bool window_open = true;
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;

    if (ImGui::Begin("Configuracion - Sonic & All-Stars Racing Transformed", &window_open, flags)) {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Sonic & All-Stars Racing Transformed - Ajustes del Sistema");
      ImGui::Separator();
      ImGui::Spacing();

      if (ImGui::BeginTabBar("ConfigTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Pantalla y Resolucion")) {
          DrawDisplayTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mandos y Controles")) {
          DrawGamepadTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Teclado y Atajos")) {
          DrawKeyboardTab();
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Acerca del Port")) {
          DrawAboutTab();
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      // Bottom control bar
      if (!save_status_.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", save_status_.c_str());
      } else {
        ImGui::TextDisabled("Atajo rapido: Presiona F1 en cualquier momento para abrir/cerrar este menu.");
      }

      ImGui::Spacing();
      if (ImGui::Button("Guardar Configuracion", ImVec2(170, 32))) {
        ApplyAndSaveSettings();
        save_status_ = "Guardado en sonicracingtransformed.toml!";
      }

      ImGui::SameLine();
      if (ImGui::Button("Guardar y Jugar", ImVec2(140, 32))) {
        ApplyAndSaveSettings();
        should_close_ = true;
        Close();
      }

      ImGui::SameLine();
      if (ImGui::Button("Restablecer", ImVec2(100, 32))) {
        ResetDefaults();
        save_status_ = "Valores restablecidos por defecto.";
      }

      ImGui::SameLine();
      if (ImGui::Button("Cerrar", ImVec2(80, 32))) {
        should_close_ = true;
        Close();
      }
    }
    ImGui::End();

    if (!window_open) {
      should_close_ = true;
      Close();
    }
  }

 private:
  void LoadCurrentSettings() {
    curr_width_ = REXCVAR_GET(window_width);
    curr_height_ = REXCVAR_GET(window_height);
    if (curr_width_ <= 0) curr_width_ = 1920;
    if (curr_height_ <= 0) curr_height_ = 1080;

    fullscreen_ = REXCVAR_GET(fullscreen);
    vsync_ = REXCVAR_GET(vsync);
    res_scale_ = REXCVAR_GET(resolution_scale);
    if (res_scale_ < 1) res_scale_ = 1;

    try {
      std::string sharpness_str = rex::cvar::GetFlagByName("present_cas_additional_sharpness");
      if (!sharpness_str.empty()) {
        cas_sharpness_ = std::stof(sharpness_str);
      }
    } catch (...) {}

    anisotropic_ = REXCVAR_GET(anisotropic_override);

    std::string effect = rex::cvar::GetFlagByName("present_effect");
    cas_enabled_ = (effect == "cas");

    // Match preset
    selected_preset_ = 5; // Custom default
    if (curr_width_ == 1280 && curr_height_ == 720) selected_preset_ = 0;
    else if (curr_width_ == 1600 && curr_height_ == 900) selected_preset_ = 1;
    else if (curr_width_ == 1920 && curr_height_ == 1080) selected_preset_ = 2;
    else if (curr_width_ == 2560 && curr_height_ == 1440) selected_preset_ = 3;
    else if (curr_width_ == 3840 && curr_height_ == 2160) selected_preset_ = 4;
  }

  void DrawDisplayTab() {
    ImGui::Text("Ajustes de Video y Resolucion");
    ImGui::Spacing();

    const char* presets[] = {
      "1280 x 720  (720p HD - Nativo Xbox 360)",
      "1600 x 900  (900p HD+)",
      "1920 x 1080 (1080p Full HD - Recomendado)",
      "2560 x 1440 (1440p 2K QHD)",
      "3840 x 2160 (4K Ultra HD)",
      "Personalizada..."
    };

    if (ImGui::Combo("Resolucion de Pantalla", &selected_preset_, presets, IM_ARRAYSIZE(presets))) {
      switch (selected_preset_) {
        case 0: curr_width_ = 1280; curr_height_ = 720; break;
        case 1: curr_width_ = 1600; curr_height_ = 900; break;
        case 2: curr_width_ = 1920; curr_height_ = 1080; break;
        case 3: curr_width_ = 2560; curr_height_ = 1440; break;
        case 4: curr_width_ = 3840; curr_height_ = 2160; break;
        default: break;
      }
      save_status_.clear();
    }

    if (selected_preset_ == 5) {
      ImGui::Indent();
      ImGui::InputInt("Ancho (Width)", &curr_width_);
      ImGui::InputInt("Alto (Height)", &curr_height_);
      if (curr_width_ < 640) curr_width_ = 640;
      if (curr_height_ < 480) curr_height_ = 480;
      ImGui::Unindent();
    }

    ImGui::Spacing();
    if (ImGui::Checkbox("Pantalla Completa (Fullscreen)", &fullscreen_)) {
      if (window_) {
        window_->SetFullscreen(fullscreen_);
      }
      save_status_.clear();
    }

    if (ImGui::Checkbox("Sincronizacion Vertical (VSync - 60 FPS estables)", &vsync_)) {
      save_status_.clear();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Calidad Grafica y Renderizado");

    if (ImGui::SliderInt("Escala Interna (Resolution Scale)", &res_scale_, 1, 3, "%dx")) {
      save_status_.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(1x = 720p nativo, 2x = 1440p, 3x = 4K)");

    if (ImGui::Checkbox("Filtro de Nitidez AMD FidelityFX CAS", &cas_enabled_)) {
      save_status_.clear();
    }

    if (cas_enabled_) {
      ImGui::Indent();
      if (ImGui::SliderFloat("Intensidad de Nitidez", &cas_sharpness_, 0.0f, 1.0f, "%.2f")) {
        save_status_.clear();
      }
      ImGui::Unindent();
    }

    const char* aniso_options[] = { "Desactivado (0x)", "2x", "4x", "8x", "16x (Maximo detalle)" };
    int aniso_idx = 0;
    if (anisotropic_ == 2) aniso_idx = 1;
    else if (anisotropic_ == 4) aniso_idx = 2;
    else if (anisotropic_ == 8) aniso_idx = 3;
    else if (anisotropic_ == 16) aniso_idx = 4;

    if (ImGui::Combo("Filtrado Anisotropico", &aniso_idx, aniso_options, IM_ARRAYSIZE(aniso_options))) {
      int mapping[] = { 0, 2, 4, 8, 16 };
      anisotropic_ = mapping[aniso_idx];
      save_status_.clear();
    }
  }

  void DrawGamepadTab() {
    ImGui::Text("Deteccion y Prueba de Mandos (SDL3 / XInput)");
    ImGui::Spacing();

    int gamepad_count = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads(&gamepad_count);

    if (gamepad_count <= 0 || joysticks == nullptr) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No se detectaron mandos conectados por USB o Bluetooth.");
      ImGui::TextWrapped("Conecta un mando de Xbox, PlayStation o generico, o utiliza el Teclado y Raton.");
      ImGui::Spacing();
      if (joysticks) SDL_free(joysticks);
      return;
    }

    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "Mandos detectados: %d", gamepad_count);

    if (selected_gamepad_index_ >= gamepad_count) {
      selected_gamepad_index_ = 0;
    }

    // Dropdown for selecting which gamepad to monitor
    std::vector<std::string> pad_names;
    std::vector<const char*> pad_cstrs;
    for (int i = 0; i < gamepad_count; ++i) {
      const char* name = SDL_GetGamepadNameForID(joysticks[i]);
      std::string label = "Jugador " + std::to_string(i + 1) + ": " + (name ? name : "Mando Desconocido");
      pad_names.push_back(label);
    }
    for (const auto& s : pad_names) {
      pad_cstrs.push_back(s.c_str());
    }

    ImGui::Combo("Mando Activo", &selected_gamepad_index_, pad_cstrs.data(), static_cast<int>(pad_cstrs.size()));
    ImGui::Spacing();

    // Open the selected gamepad to inspect real-time values
    SDL_Gamepad* gamepad = SDL_OpenGamepad(joysticks[selected_gamepad_index_]);
    if (!gamepad) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No se pudo abrir el mando seleccionado.");
      SDL_free(joysticks);
      return;
    }

    ImGui::Text("Nombre: %s", SDL_GetGamepadName(gamepad));
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Estado: Conectado y Transmitiendo");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Monitor de Entradas en Tiempo Real (Mueve o presiona botones en tu mando):");
    ImGui::Spacing();

    // Read axes
    float left_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float left_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    float right_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    float right_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;

    float trigger_l = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    float trigger_r = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    if (trigger_l < 0.0f) trigger_l = 0.0f;
    if (trigger_r < 0.0f) trigger_r = 0.0f;

    // Triggers
    ImGui::Text("Gatillos Analogicos:");
    ImGui::ProgressBar(trigger_l, ImVec2(240, 20), "Gatillo Izquierdo (Freno/Reversa)");
    ImGui::SameLine();
    ImGui::ProgressBar(trigger_r, ImVec2(240, 20), "Gatillo Derecho (Acelerar)");

    ImGui::Spacing();
    // Sticks
    ImGui::Text("Sticks Analogicos:");
    ImGui::SliderFloat("Stick Izq X (Direccion)", &left_x, -1.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput);
    ImGui::SliderFloat("Stick Izq Y", &left_y, -1.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput);
    ImGui::SliderFloat("Stick Der X (Camara)", &right_x, -1.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput);
    ImGui::SliderFloat("Stick Der Y", &right_y, -1.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput);

    ImGui::Spacing();
    ImGui::Text("Botones Principales (Se iluminan en verde al presionar):");
    ImGui::Spacing();

    auto DrawButtonBadge = [](const char* name, bool pressed) {
      if (pressed) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.8f, 0.2f, 1.0f));
        ImGui::Button(name, ImVec2(75, 30));
        ImGui::PopStyleColor();
      } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::Button(name, ImVec2(75, 30));
        ImGui::PopStyleColor();
      }
    };

    bool btn_a = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    bool btn_b = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
    bool btn_x = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST);
    bool btn_y = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH);
    bool btn_lb = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    bool btn_rb = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    bool btn_start = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
    bool btn_back = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK);
    bool dpad_u = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    bool dpad_d = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    bool dpad_l = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    bool dpad_r = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

    DrawButtonBadge("A / Cruz", btn_a); ImGui::SameLine();
    DrawButtonBadge("B / Circulo", btn_b); ImGui::SameLine();
    DrawButtonBadge("X / Cuad", btn_x); ImGui::SameLine();
    DrawButtonBadge("Y / Triang", btn_y); ImGui::SameLine();
    DrawButtonBadge("LB / L1", btn_lb); ImGui::SameLine();
    DrawButtonBadge("RB / R1", btn_rb);

    ImGui::Spacing();
    DrawButtonBadge("Start", btn_start); ImGui::SameLine();
    DrawButtonBadge("Back/Sel", btn_back); ImGui::SameLine();
    DrawButtonBadge("D-Pad Arriba", dpad_u); ImGui::SameLine();
    DrawButtonBadge("D-Pad Abajo", dpad_d); ImGui::SameLine();
    DrawButtonBadge("D-Pad Izq", dpad_l); ImGui::SameLine();
    DrawButtonBadge("D-Pad Der", dpad_r);

    SDL_CloseGamepad(gamepad);
    SDL_free(joysticks);
  }

  void DrawKeyboardTab() {
    ImGui::Text("Controles de Teclado y Raton (MnK)");
    ImGui::Spacing();

    ImGui::BeginTable("KeyTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
    ImGui::TableSetupColumn("Accion en el Juego", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Tecla Asignada");
    ImGui::TableHeadersRow();

    auto AddRow = [](const char* action, const char* key) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s", action);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "%s", key);
    };

    AddRow("Acelerar (Accelerate)", "W  /  Flecha Arriba  /  Gatillo RT");
    AddRow("Frenar / Reversa (Brake)", "S  /  Flecha Abajo  /  Gatillo LT");
    AddRow("Girar a la Izquierda", "A  /  Flecha Izquierda  /  Stick L");
    AddRow("Girar a la Derecha", "D  /  Flecha Derecha  /  Stick L");
    AddRow("Derrape / Salto (Drift)", "Espacio  /  Shift Izquierdo  /  Boton X");
    AddRow("Usar Objeto / Poder (Item)", "E  /  Ctrl Izquierdo  /  Boton B");
    AddRow("Mirar Atras (Look Back)", "Q  /  Boton Y");
    AddRow("Pausa / Menu de Carrera", "Escape  /  Enter  /  Boton Start");
    AddRow("Menu de Configuracion (Este)", "F1  (o F4)");
    AddRow("Monitor de Rendimiento FPS", "F3");
    AddRow("Consola del Sistema", "Tecla ~ (Backtick)");

    ImGui::EndTable();
  }

  void DrawAboutTab() {
    ImGui::Text("Sonic & All-Stars Racing Transformed");
    ImGui::Text("Proyecto de Recompilacion Estatica Nativa (ReXGlue SDK)");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BulletText("Plataformas destino: PC (Windows / Linux) y Android (Snapdragon 8 Elite / Vulkan).");
    ImGui::BulletText("Motor grafico activo: Direct3D 12 (NVIDIA GeForce RTX 5060).");
    ImGui::BulletText("Subsistema de sonido: SDL 3.5.0 Audio System con decodificacion XMA nativa.");
    ImGui::BulletText("Entradas: SDL3 MnK + Gamepad nativo con vibracion y reconocimiento automatico.");
  }

  void ApplyAndSaveSettings() {
    REXCVAR_SET(window_width, curr_width_);
    REXCVAR_SET(window_height, curr_height_);
    REXCVAR_SET(fullscreen, fullscreen_);
    REXCVAR_SET(vsync, vsync_);
    REXCVAR_SET(resolution_scale, res_scale_);
    rex::cvar::SetFlagByName("present_cas_additional_sharpness", std::to_string(cas_sharpness_));
    REXCVAR_SET(anisotropic_override, anisotropic_);
    rex::cvar::SetFlagByName("present_effect", cas_enabled_ ? "cas" : "");

    if (window_) {
      window_->SetFullscreen(fullscreen_);
    }

    rex::cvar::SaveConfig(config_path_);
    REXLOG_INFO("Configuracion guardada exitosamente en {}", config_path_.string());
  }

  void ResetDefaults() {
    curr_width_ = 1920;
    curr_height_ = 1080;
    fullscreen_ = false;
    vsync_ = true;
    res_scale_ = 1;
    cas_enabled_ = true;
    cas_sharpness_ = 0.5f;
    anisotropic_ = 16;
    selected_preset_ = 2;
  }

  std::filesystem::path config_path_;
  rex::ui::Window* window_ = nullptr;
  bool should_close_ = false;
  std::string save_status_;

  int selected_preset_ = 2;
  int curr_width_ = 1920;
  int curr_height_ = 1080;
  bool fullscreen_ = false;
  bool vsync_ = true;
  int res_scale_ = 1;
  bool cas_enabled_ = true;
  float cas_sharpness_ = 0.5f;
  int anisotropic_ = 16;
  int selected_gamepad_index_ = 0;
};
