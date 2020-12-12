#include "esphome.h"
#include <ArduinoJson.h>
#include <sstream>

using namespace esphome;

#define SCRIPTURE_URL "http://beta.ourmanna.com/api/v1/get/?format=json&order=random"
#define MAX_VERSE_LINES 8
#define MAX_CHAR_PER_LINE 35
#define STARTING_POSITION 40
#define SPACE_BETWEEN_LINES 50
#define JSON_BUFFER_SIZE (JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(1))

class ScriptureOfTheDay : public Component {
  public:
    float get_setup_priority() const override { return setup_priority::LATE; }
    void setup() override { update_at_ = millis() + 6000; }
    void loop() override;

    void set_http_request(http_request::HttpRequestComponent *http_request) { http_request_ = http_request; }
    void set_display(waveshare_epaper::WaveshareEPaper *display) { display_ = display; }
    void set_deep_sleep(deep_sleep::DeepSleepComponent *deep_sleep) { deep_sleep_ = deep_sleep; }
    void set_sntp(sntp::SNTPComponent *sntp) { sntp_ = sntp; }
    void set_fonts(display::Font *verse_font, display::Font *reference_font) { verse_font_ = verse_font; reference_font_ = reference_font; }

    void display_scripture();

  protected:
    http_request::HttpRequestComponent *http_request_;
    waveshare_epaper::WaveshareEPaper *display_;
    deep_sleep::DeepSleepComponent *deep_sleep_;
    sntp::SNTPComponent *sntp_;
    display::Font *verse_font_;
    display::Font *reference_font_;

    std::vector<std::string> scripture_;
    uint64_t update_at_; 

    std::vector<std::string> get_scripture();
    std::vector<std::string> split_verse_into_lines(std::string verse);
    void shutdown(uint32_t sleepSeconds = 0);
};

void ScriptureOfTheDay::loop()
{
  // If we try to make the request to soon it fails so we delay 6 seconds
  if (millis() > update_at_)
  {
    // If we get an especially long verse that won't fit on the display we try again.
    do
    {
      scripture_ = get_scripture();
    } while (scripture_.size() > MAX_VERSE_LINES + 1);

    display_->update();

    shutdown();
  }
}

void ScriptureOfTheDay::display_scripture()
{
  // Don't do anything if we haven't gotten the new scripture yet
  if (scripture_.size() == 0)
  {
    return;
  }

  int verseLineCount = scripture_.size() - 1;
  int currentPosition = STARTING_POSITION + (SPACE_BETWEEN_LINES / 2) * (MAX_VERSE_LINES - verseLineCount);
  for (int i = 0; i < verseLineCount; i++)
  {
    display_->print(400, currentPosition, verse_font_, TextAlign::CENTER, scripture_[i].c_str());
    currentPosition += SPACE_BETWEEN_LINES;
  }

  display_->print(760, currentPosition, reference_font_, TextAlign::CENTER_RIGHT, scripture_[verseLineCount].c_str());
}

std::vector<std::string> ScriptureOfTheDay::get_scripture()
{
  http_request_->set_url(SCRIPTURE_URL);
  http_request_->set_method("GET");
  
  http_request_->send();
  if (http_request_->status_has_warning())
  {
    // If the request failed we shutdown and try again in 15 minutes
    shutdown(15 * 60);
  }
  const char *json = http_request_->get_string();
  
  DynamicJsonBuffer jsonBuffer(JSON_BUFFER_SIZE);
  JsonObject& scripture = jsonBuffer.parseObject(json);
  
  http_request_->close();

  std::vector<std::string> formattedScripture = split_verse_into_lines(std::string(scripture["verse"]["details"]["text"].as<char*>()));
  formattedScripture.push_back(std::string(scripture["verse"]["details"]["reference"].as<char*>()));

  return formattedScripture;
}

std::vector<std::string> ScriptureOfTheDay::split_verse_into_lines(std::string verse)
{
  std::string word;
  std::string currentLine;
  std::vector<std::string> lines;
  std::istringstream ss(verse);
  
  while (ss >> word)
  {
    if (currentLine.length() == 0)
    {
      currentLine = word;
    }
    else if (currentLine.length() + word.length() < MAX_CHAR_PER_LINE)
    {
      currentLine += " " + word;
    }
    else
    {
      lines.push_back(currentLine);
      currentLine = word;
    }
  }

  lines.push_back(currentLine);
  
  return lines;
}

void ScriptureOfTheDay::shutdown(uint32_t sleepSeconds)
{
  if (sleepSeconds == 0)
  {
    // Wake back up and update at 3AM
    time::ESPTime now = sntp_->now();
    sleepSeconds = 24 * 60 * 60 - (now.hour - 3) * 60 * 60 - now.minute * 60 - now.second;
  }

  deep_sleep_->set_sleep_duration(sleepSeconds * 1000);
  deep_sleep_->begin_sleep(true);
}

ScriptureOfTheDay *Scripture;
