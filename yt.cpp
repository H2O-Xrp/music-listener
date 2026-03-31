#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <unistd.h>
#include <sstream>
#include <cstring>
#include <sys/stat.h>
#include <signal.h>

struct VideoResult {
    std::string title;
    std::string url;
    std::string video_id;
    std::string duration;
    int index;
};

class YouTubeMusicPlayer {
private:
    std::vector<VideoResult> current_results;
    int current_page = 0;
    int total_pages = 0;
    const int ITEMS_PER_PAGE = 10;
    std::string last_query;
    std::string cookie_path;
    bool has_cookies = false;
    bool repeat_mode = false;
    bool is_playing = false;

    void init() {
        const char* home = getenv("HOME");
        if (!home) home = ".";

        cookie_path = std::string(home) + "/cookies.txt";
        has_cookies = (access(cookie_path.c_str(), F_OK) == 0);

        if (has_cookies) {
            chmod(cookie_path.c_str(), 0600);
        }

        if (system("which mpv > /dev/null 2>&1") != 0) {
            std::cout << "⚠ mpv not found! Install: pkg install mpv\n";
        }
    }

    std::string execCommand(const std::string& cmd) {
        std::string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
        pclose(pipe);
        return result;
    }

    std::vector<VideoResult> searchYouTube(const std::string& query) {
        std::vector<VideoResult> results;

        std::string cmd;
        if (has_cookies) {
            cmd = "yt-dlp --cookies \"" + cookie_path + "\" "
                  "--flat-playlist --print \"%(title)s|%(url)s|%(duration)s|%(id)s\" "
                  "--playlist-end 30 \"ytsearch30:" + query + "\" 2>/dev/null";
        } else {
            cmd = "yt-dlp --flat-playlist --print \"%(title)s|%(url)s|%(duration)s|%(id)s\" "
                  "--playlist-end 30 \"ytsearch30:" + query + "\" 2>/dev/null";
        }

        std::string output = execCommand(cmd);
        if (output.empty()) return results;

        std::stringstream ss(output);
        std::string line;
        int index = 1;

        while (std::getline(ss, line) && results.size() < 30) {
            if (line.empty()) continue;

            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            size_t pos3 = line.find('|', pos2 + 1);

            if (pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos) {
                VideoResult v;
                v.title = line.substr(0, pos1);
                v.url = line.substr(pos1 + 1, pos2 - pos1 - 1);
                v.duration = line.substr(pos2 + 1, pos3 - pos2 - 1);
                v.video_id = line.substr(pos3 + 1);
                v.index = index++;

                v.title.erase(std::remove(v.title.begin(), v.title.end(), '\r'), v.title.end());

                if (v.duration.find("http") != std::string::npos || v.duration.length() > 10) {
                    v.duration = "?";
                }

                results.push_back(v);
            }
        }

        return results;
    }

    void playWithMpv(const std::string& url) {
        std::string cmd;
        if (has_cookies) {
            cmd = "mpv --no-video --cookies-file=\"" + cookie_path + "\" \"" + url + "\"";
        } else {
            cmd = "mpv --no-video \"" + url + "\"";
        }

        system(cmd.c_str());
    }

    void playWithAutoRepeat(const VideoResult& video) {
        int count = 1;
        is_playing = true;

        std::cout << "\n🔁 AUTO REPLAY MODE ACTIVE\n";
        std::cout << "   Press Ctrl+C to stop\n";
        std::cout << "═══════════════════════════════════════\n";

        while (repeat_mode && is_playing) {
            std::cout << "\n🎵 [" << count << "] " << video.title << std::endl;
            playWithMpv(video.url);
            count++;

            if (repeat_mode) {
                std::cout << "⏭ Replaying in 1 second... (Press Ctrl+C to stop)\n";
                sleep(1);
            }
        }

        is_playing = false;
    }

    std::string formatDuration(const std::string& duration_str) {
        if (duration_str.empty() || duration_str == "?") return "?";
        try {
            int seconds = std::stoi(duration_str);
            int minutes = seconds / 60;
            int secs = seconds % 60;
            char buffer[10];
            sprintf(buffer, "%d:%02d", minutes, secs);
            return std::string(buffer);
        } catch (...) {
            return "?";
        }
    }

    void displayResults() {
        if (current_results.empty()) {
            std::cout << "\n❌ No results!\n";
            return;
        }

        int start = current_page * ITEMS_PER_PAGE;
        int end = std::min(start + ITEMS_PER_PAGE, (int)current_results.size());

        system("clear");

        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  🎵 " << last_query;
        for(int i = 0; i < 58 - (int)last_query.length(); i++) std::cout << " ";
        std::cout << "║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";

        for (int i = start; i < end; i++) {
            const auto& video = current_results[i];
            std::string duration = formatDuration(video.duration);
            int display_num = (i - start) + 1;

            std::string title = video.title;
            if (title.length() > 48) title = title.substr(0, 45) + "...";

            printf("║  %2d. %-48s ⏱%s║\n", display_num, title.c_str(), duration.c_str());
        }

        for (int i = end - start; i < ITEMS_PER_PAGE; i++) {
            printf("║  %2d. %-54s ║\n", i+1, "-");
        }

        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";

        total_pages = (current_results.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
        printf("║  Page %d of %d", current_page + 1, total_pages);
        int spacing = 58 - (12 + std::to_string(current_page+1).length() + std::to_string(total_pages).length());
        for(int i = 0; i < spacing; i++) std::cout << " ";
        std::cout << "║\n";

        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Commands:                                                      ║\n";
        std::cout << "║  [1-10] Play song                                               ║\n";
        std::cout << "║  !REPEAT ON      - Auto repeat (Ctrl+C to stop)                 ║\n";
        std::cout << "║  !REPEAT OFF     - Disable repeat mode                          ║\n";
        std::cout << "║  !SEARCH <query> - Search new song                              ║\n";
        std::cout << "║  !NEXT           - Next page                                    ║\n";
        std::cout << "║  !PREV           - Previous page                                ║\n";
        std::cout << "║  !HELP           - Show help                                    ║\n";
        std::cout << "║  !QUIT           - Exit program                                 ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";

        // Status
        std::cout << "║  🍪 Cookies: " << (has_cookies ? "✓" : "✗");
        for(int i = 0; i < 57 - (has_cookies ? 11 : 11); i++) std::cout << " ";
        std::cout << "║\n";

        std::cout << "║  🔁 Repeat: " << (repeat_mode ? "ON (auto replay)" : "OFF");
        for(int i = 0; i < 53 - (repeat_mode ? 20 : 12); i++) std::cout << " ";
        std::cout << "║\n";

        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n> ";
        std::cout.flush();
    }

    void showHelp() {
        std::cout << "\n📖 Command List:\n";
        std::cout << "═══════════════════════════════════════════════════════════════\n";
        std::cout << "  [1-10]            - Play selected song\n";
        std::cout << "  !REPEAT ON        - Enable auto repeat (Ctrl+C to stop)\n";
        std::cout << "  !REPEAT OFF       - Disable repeat mode\n";
        std::cout << "  !SEARCH <query>   - Search for new songs\n";
        std::cout << "  !NEXT             - Go to next page\n";
        std::cout << "  !PREV             - Go to previous page\n";
        std::cout << "  !HELP             - Show this help\n";
        std::cout << "  !QUIT             - Exit program\n";
        std::cout << "═══════════════════════════════════════════════════════════════\n";
        std::cout << "💡 Auto Repeat: Once enabled, song will replay automatically\n";
        std::cout << "   Press Ctrl+C to stop playback and return to menu\n";
        std::cout << "💡 !SEARCH: Example - !SEARCH chiisana koi no uta\n";
    }

    void processCommand(const std::string& cmd) {
        std::string upper_cmd = cmd;
        std::transform(upper_cmd.begin(), upper_cmd.end(), upper_cmd.begin(), ::toupper);

        // Handle !SEARCH command (dengan query)
        if (upper_cmd.substr(0, 7) == "!SEARCH") {
            std::string query = cmd.substr(7);
            // Trim leading spaces
            size_t start = query.find_first_not_of(" \t");
            if (start != std::string::npos) {
                query = query.substr(start);
            } else {
                std::cout << "❌ Please provide a search query\n";
                std::cout << "   Example: !SEARCH chiisana koi no uta\n";
                sleep(1);
                return;
            }

            if (query.empty()) {
                std::cout << "❌ Please provide a search query\n";
                sleep(1);
                return;
            }

            std::cout << "\n🔎 Searching: " << query << "\n";
            last_query = query;
            current_results = searchYouTube(query);
            current_page = 0;

            if (current_results.empty()) {
                std::cout << "❌ No results found.\n";
                last_query = "";
            }
            return;
        }

        // Other commands
        if (upper_cmd == "!REPEAT ON") {
            repeat_mode = true;
            std::cout << "✓ Auto Repeat: ON\n";
            std::cout << "  Songs will replay automatically. Press Ctrl+C to stop.\n";
        }
        else if (upper_cmd == "!REPEAT OFF") {
            repeat_mode = false;
            std::cout << "✓ Auto Repeat: OFF\n";
        }
        else if (upper_cmd == "!NEXT") {
            if (current_page + 1 < total_pages) {
                current_page++;
                std::cout << "✓ Page " << current_page+1 << " of " << total_pages << "\n";
            } else {
                std::cout << "⚠ Already on last page!\n";
            }
        }
        else if (upper_cmd == "!PREV") {
            if (current_page > 0) {
                current_page--;
                std::cout << "✓ Page " << current_page+1 << " of " << total_pages << "\n";
            } else {
                std::cout << "⚠ Already on first page!\n";
            }
        }
        else if (upper_cmd == "!HELP") {
            showHelp();
        }
        else if (upper_cmd == "!QUIT") {
            std::cout << "👋 Goodbye!\n";
            exit(0);
        }
        else {
            std::cout << "❌ Unknown command: " << cmd << "\n";
            std::cout << "Type !HELP for available commands\n";
        }

        sleep(1);
    }

    std::string getInput() {
        std::string input;
        std::getline(std::cin, input);
        return input;
    }

public:
    YouTubeMusicPlayer() {
        init();
    }

    void run() {
        std::string input;

        while (true) {
            if (last_query.empty()) {
                std::cout << "\n🎵 YouTube Music Player (Auto Repeat + !SEARCH)\n";
                std::cout << "═══════════════════════════════════════════════════════\n";
                std::cout << "🍪 Cookies: " << (has_cookies ? "✓" : "✗") << "\n";
                std::cout << "🎚 Player: mpv (streaming, no download)\n";
                std::cout << "🔁 Auto Repeat: " << (repeat_mode ? "ON (Ctrl+C to stop)" : "OFF") << "\n";
                std::cout << "═══════════════════════════════════════════════════════\n";
                std::cout << "💡 Type !HELP for commands\n";
                std::cout << "🔍 Search: ";
                std::cout.flush();

                input = getInput();
                if (input.empty()) continue;

                if (input[0] == '!') {
                    processCommand(input);
                    continue;
                }

                if (input == "q" || input == "quit") break;

                last_query = input;
                std::cout << "\n🔎 Searching...\n";
                current_results = searchYouTube(input);
                current_page = 0;

                if (current_results.empty()) {
                    std::cout << "❌ No results found.\n";
                    last_query = "";
                }
                continue;
            }

            displayResults();

            input = getInput();

            if (!input.empty() && input[0] == '!') {
                processCommand(input);
                continue;
            }

            bool is_number = !input.empty() && std::all_of(input.begin(), input.end(), ::isdigit);

            if (is_number) {
                int num = std::stoi(input);
                if (num >= 1 && num <= ITEMS_PER_PAGE) {
                    int idx = current_page * ITEMS_PER_PAGE + (num - 1);
                    if (idx < current_results.size()) {
                        if (repeat_mode) {
                            playWithAutoRepeat(current_results[idx]);
                        } else {
                            std::cout << "\n🎵 Now playing...\n";
                            std::cout << "⏹ Press Ctrl+C to stop\n" << std::endl;
                            playWithMpv(current_results[idx].url);
                            std::cout << "\n✓ Playback finished. Press Enter...";
                            std::cin.get();
                        }
                    }
                }
            }
            else if (!input.empty()) {
                std::cout << "❌ Invalid input. Type !HELP for commands\n";
                sleep(1);
            }
        }

        std::cout << "\n👋 Goodbye!\n";
    }
};

int main() {
    YouTubeMusicPlayer player;
    player.run();
    return 0;
}
