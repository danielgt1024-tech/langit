#include <iostream>
#include <cstdint>
#include <cstddef>
#include <vector>

// ============================================================================
// 1. ARCHITECTURE BOUNDARIES & CONFIGURATION
// ============================================================================
constexpr uint8_t OS_CTRL_MIN = 0x00;
constexpr uint8_t OS_CTRL_MAX = 0x1F;
constexpr uint8_t OS_CHAR_MIN = 0x20;
constexpr uint8_t OS_CHAR_MAX = 0x7F;
constexpr uint8_t OS_MOD_MIN  = 0x80;
constexpr uint8_t OS_MOD_MAX  = 0xFF;

// Control Codes
constexpr uint8_t CTRL_BS = 0x08; // \b Backspace
constexpr uint8_t CTRL_LF = 0x0A; // \n Line Feed
constexpr uint8_t CTRL_CR = 0x0D; // \r Carriage Return

// Font Metrics
constexpr size_t GLYPH_WIDTH  = 8;
constexpr size_t GLYPH_HEIGHT = 16;
constexpr size_t BYTES_PER_GLYPH = GLYPH_HEIGHT; 
constexpr size_t GLYPHS_PER_PAGE = 96;          
constexpr size_t TOTAL_FONT_PAGES = 129;         

// Terminal Layout Dimensions
constexpr size_t SCREEN_COLS = 40; 
constexpr size_t SCREEN_ROWS = 10; // Bumped slightly to show color logs cleanly

enum class StreamStatus {
    HandledInternal,
    CellReady,
    TriggerNewline,
    TriggerCarriageReturn,
    TriggerBackspace
};

// 16-Bit Color Codes Map (Simple 4-bit ANSI scheme stored inside your 16-bit fields)
constexpr uint16_t COLOR_BLACK   = 0;
constexpr uint16_t COLOR_RED     = 1;
constexpr uint16_t COLOR_GREEN   = 2;
constexpr uint16_t COLOR_YELLOW  = 3;
constexpr uint16_t COLOR_BLUE    = 4;
constexpr uint16_t COLOR_MAGENTA = 5;
constexpr uint16_t COLOR_CYAN    = 6;
constexpr uint16_t COLOR_WHITE   = 7;

struct alignas(uint64_t) ScreenCell {
    uint8_t  character_code;   
    uint8_t  font_page;        
    uint16_t foreground_color; 
    uint16_t background_color; 
    uint16_t attributes;       
};

// ============================================================================
// 2. FONT PIPELINE & COMPILER UTILITY
// ============================================================================
class FontPipeline {
private:
    uint8_t font_rom[TOTAL_FONT_PAGES * GLYPHS_PER_PAGE * BYTES_PER_GLYPH];
    uint8_t active_font_page = 0; 

public:
    FontPipeline() : font_rom{} {}

    // FONT COMPILER: Takes raw glyph parameters and builds the kernel font ROM sequence
    void compile_and_load_glyph(uint8_t target_page, char character, const std::vector<uint8_t>& bitmap_rows) {
        if (target_page >= TOTAL_FONT_PAGES || character < OS_CHAR_MIN || character > OS_CHAR_MAX) return;
        
        size_t char_offset = character - OS_CHAR_MIN;
        size_t global_glyph_index = (target_page * GLYPHS_PER_PAGE) + char_offset;
        size_t byte_address = global_glyph_index * BYTES_PER_GLYPH;

        for (size_t i = 0; i < BYTES_PER_GLYPH && i < bitmap_rows.size(); ++i) {
            font_rom[byte_address + i] = bitmap_rows[i];
        }
    }

    StreamStatus process_stream_byte(uint8_t input_byte, uint16_t fg, uint16_t bg, ScreenCell& out_cell) {
        if (input_byte <= OS_CTRL_MAX) {
            if (input_byte == CTRL_LF) return StreamStatus::TriggerNewline;
            if (input_byte == CTRL_CR) return StreamStatus::TriggerCarriageReturn;
            if (input_byte == CTRL_BS) return StreamStatus::TriggerBackspace;
            return StreamStatus::HandledInternal;
        }
        if (input_byte >= OS_MOD_MIN && input_byte <= OS_MOD_MAX) {
            active_font_page = (input_byte - OS_MOD_MIN) + 1;
            return StreamStatus::HandledInternal; 
        }
        if (input_byte >= OS_CHAR_MIN && input_byte <= OS_CHAR_MAX) {
            out_cell.character_code  = input_byte;
            out_cell.font_page       = active_font_page;
            out_cell.foreground_color = fg;
            out_cell.background_color = bg;
            out_cell.attributes       = 0; 
            return StreamStatus::CellReady;
        }
        return StreamStatus::HandledInternal;
    }

    uint8_t get_active_page() const { return active_font_page; }
};

// ============================================================================
// 3. TERMINAL BUFFER SUBSYSTEM (WITH ANSI & BACKSPACE MULTI-WRAP)
// ============================================================================
class Terminal {
private:
    ScreenCell grid[SCREEN_ROWS * SCREEN_COLS];
    size_t cursor_x = 0;
    size_t cursor_y = 0;

    // Translates internal 16-bit color values to host ANSI escapes sequences
    void apply_ansi_colors(uint16_t fg, uint16_t bg) const {
        std::cout << "\033[" << (30 + (fg & 0x7)) << ";" << (40 + (bg & 0x7)) << "m";
    }

    void reset_ansi_colors() const {
        std::cout << "\033[0m";
    }

public:
    Terminal() {
        clear_screen();
    }

    void clear_screen() {
        ScreenCell empty_cell = {0x20, 0, COLOR_WHITE, COLOR_BLACK, 0}; 
        for (size_t i = 0; i < SCREEN_ROWS * SCREEN_COLS; ++i) {
            grid[i] = empty_cell;
        }
        cursor_x = 0;
        cursor_y = 0;
    }

    void write_cell(const ScreenCell& cell) {
        if (cursor_y >= SCREEN_ROWS || cursor_x >= SCREEN_COLS) return;
        grid[cursor_y * SCREEN_COLS + cursor_x] = cell;
        cursor_x++;
        if (cursor_x >= SCREEN_COLS) {
            newline();
        }
    }

    // BACKSPACE ENGINE: Safely walks backward across lines
    void backspace() {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = SCREEN_COLS - 1; // Wrap backward to end of previous line
        } else {
            return; // Already at (0,0), nothing to do
        }
        // Erase character under the new cursor location
        grid[cursor_y * SCREEN_COLS + cursor_x] = {0x20, 0, COLOR_WHITE, COLOR_BLACK, 0};
    }

    void carriage_return() {
        cursor_x = 0;
    }

    void newline() {
        cursor_x = 0; 
        if (cursor_y < SCREEN_ROWS - 1) {
            cursor_y++;
        } else {
            scroll_down();
        }
    }

    void scroll_down() {
        for (size_t r = 1; r < SCREEN_ROWS; ++r) {
            for (size_t c = 0; c < SCREEN_COLS; ++c) {
                grid[(r - 1) * SCREEN_COLS + c] = grid[r * SCREEN_COLS + c];
            }
        }
        ScreenCell blank_cell = {0x20, 0, COLOR_WHITE, COLOR_BLACK, 0};
        for (size_t c = 0; c < SCREEN_COLS; ++c) {
            grid[(SCREEN_ROWS - 1) * SCREEN_COLS + c] = blank_cell;
        }
        cursor_y = SCREEN_ROWS - 1;
        cursor_x = 0;
    }

    void render_to_console() const {
        reset_ansi_colors();
        std::cout << "\n+";
        for (size_t c = 0; c < SCREEN_COLS; ++c) std::cout << "-";
        std::cout << "+\n";

        for (size_t r = 0; r < SCREEN_ROWS; ++r) {
            std::cout << "|"; 
            for (size_t c = 0; c < SCREEN_COLS; ++c) {
                if (r == cursor_y && c == cursor_x) {
                    reset_ansi_colors();
                    std::cout << "█"; 
                } else {
                    const auto& cell = grid[r * SCREEN_COLS + c];
                    apply_ansi_colors(cell.foreground_color, cell.background_color);
                    std::cout << (char)cell.character_code;
                }
            }
            reset_ansi_colors();
            std::cout << "|\n"; 
        }

        std::cout << "+";
        for (size_t c = 0; c < SCREEN_COLS; ++c) std::cout << "-";
        std::cout << "+\n";
    }
};

// ============================================================================
// 4. MAIN PROGRAM RUNNER
// ============================================================================
int main() {
    FontPipeline os_fonts;
    Terminal os_terminal;
    ScreenCell cell_output;

    // --- 1. FONT COMPILER ASSET INJECTION SIMULATION ---
    // Compile a custom binary bitmap profile for the letter 'A' inside font page index 1
    std::vector<uint8_t> custom_a_glyph = {
        0b00011000,
        0b00100100,
        0b01000010,
        0b01111110,
        0b01000010,
        0b01000010,
        0b01000010,
        0b00000000
    };
    os_fonts.compile_and_load_glyph(1, 'A', custom_a_glyph);

    // Lambda helper handling color attributes dynamically over the custom stream loops
    auto print_string_color = [&](const char* text, uint16_t fg, uint16_t bg) {
        while (*text) {
            StreamStatus status = os_fonts.process_stream_byte(static_cast<uint8_t>(*text), fg, bg, cell_output);
            
            if (status == StreamStatus::CellReady)            os_terminal.write_cell(cell_output);
            else if (status == StreamStatus::TriggerNewline)       os_terminal.newline();
            else if (status == StreamStatus::TriggerCarriageReturn) os_terminal.carriage_return();
            else if (status == StreamStatus::TriggerBackspace)      os_terminal.backspace();
            
            text++;
        }
    };

    std::cout << "=== CORE OS TEXT SUBSYSTEM INITIALIZED ===" << std::endl;

    // Test 1: ANSI Colors Rendering Demonstration
    print_string_color("Kernel Status: ", COLOR_WHITE, COLOR_BLACK);
    print_string_color("ONLINE\n", COLOR_GREEN, COLOR_BLACK);
    print_string_color("Storage Node:  ", COLOR_WHITE, COLOR_BLACK);
    print_string_color("WARNING\n", COLOR_YELLOW, COLOR_BLACK);
    print_string_color("Critical File: ", COLOR_WHITE, COLOR_BLACK);
    print_string_color("FAILED\n", COLOR_RED, COLOR_BLACK);
    os_terminal.render_to_console();

    // Test 2: Backspace Deletion Verification (Typing "Errorrr", then sending 2 backspaces to correct it)
    std::cout << "\n[Test 2] Simulating interactive user typo correction (Backspace)..." << std::endl;
    print_string_color("System Errorrr", COLOR_CYAN, COLOR_BLACK);
    print_string_color("\b\b", COLOR_WHITE, COLOR_BLACK); // Deletes the extra two 'r' elements
    os_terminal.render_to_console();

    // Test 3: Modifiers and Asset Lookup validation
    std::cout << "\n[Test 3] Activating page modifier state injection..." << std::endl;
    uint8_t stream_with_modifier[] = { 0x80, 'A', 0 }; // 0x80 shifts into compiled Page 1// Process stream byte directly to handle shift modifications safely
    int i = 0;
    while(stream_with_modifier[i]) {
        StreamStatus status = os_fonts.process_stream_byte(stream_with_modifier[i],
            COLOR_MAGENTA, COLOR_BLACK, cell_output);
        if (status == StreamStatus::CellReady) {
            os_terminal.write_cell(cell_output);
            std::cout << "Successfully parsed char code 0x" << std::hex <<
            (int)cell_output.character_code
            << " bound to compiled Font Page " << std::dec << (int)cell_output.font_page << std::endl;
        }
        i++;
    }
    os_terminal.render_to_console();
    return 0;
}