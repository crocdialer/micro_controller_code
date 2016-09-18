// SN74141 (1 + 2)
uint8_t g_control_pins[8] = {2, 3, 4, 5, 6, 7, 8, 9};

// anod pins
uint8_t g_anode_pins[4] = {10, 11, 12, 13};

#define SERIAL_BUFSIZE 32
char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
uint32_t g_counter = 0;
uint32_t g_update_interval = 450;

// our value to display on the nixies
uint16_t g_current_value = 6;

const uint32_t g_nixie_delay = 5;

void setup()
{
    for(uint8_t i = 0; i < 4; i++){ pinMode(g_anode_pins[i], OUTPUT); }
    for(uint8_t i = 0; i < 8; i++){ pinMode(g_control_pins[i], OUTPUT); }

    clear();

    memset(g_serial_buf, 0, SERIAL_BUFSIZE);

    // initialize the serial communication:
    Serial.begin(57600);
}

void display_digit(uint32_t the_digit, uint8_t the_anode, uint8_t the_cathode_bank = 0)
{
    // set approriate pin array
    uint8_t *cathod_pins = the_cathode_bank? g_control_pins + 4 : g_control_pins;

    for(int i = 0; i < 4; i++)
    {
        uint8_t val = ((the_digit % 10) >> i) & 0x1;
        digitalWrite(cathod_pins[i], val);
    }
    // turn on the anode
    digitalWrite(g_anode_pins[the_anode], HIGH);
    delay(g_nixie_delay);
    digitalWrite(g_anode_pins[the_anode], LOW);
}

void display_number(uint16_t the_number)
{
    display_digit(the_number, 0, 0);
    display_digit((the_number / 10), 1, 0);
    display_digit((the_number / 100), 2, 0);
    display_digit((the_number / 1000), 3, 0);
}

void clear()
{
    for(uint8_t i = 0; i < 4; i++){ digitalWrite(g_anode_pins[i], LOW); }
}

////////////////////////////////////////////////////////////////////////
void loop()
{
    // time measurement
    int delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    if(g_time_accum >= g_update_interval)
    {
        g_time_accum = 0;
        g_counter = (g_counter + 1) % 10000;
    }

    // test counter
    g_current_value = g_counter;

    // show something
    // display_number(g_current_value);
    display_number(millis() / 100);
}


void serialEvent()
{
    while (Serial.available())
    {
        // get the new byte:
        char c = Serial.read();

        if(c == '\0'){ continue; }

        // add it to the buf
        g_serial_buf[g_buf_index % SERIAL_BUFSIZE] = c;
        g_buf_index++;

        // if the incoming character is a newline, set a flag
        if (c == '\n')
        {
            g_current_value = atoi(g_serial_buf);
            g_buf_index = 0;
            memset(g_serial_buf, 0, SERIAL_BUFSIZE);
        }
    }
}
