//-----------------------------------------------------------------------------
// BitWidgets - A simple logic gate simulator using images as circuit blueprints.
// Runs on top of the desktop as a transparent window. Based on raylib.
// Author: github.com/SMDHuman
// License: MIT License / 13-11-2025
//-----------------------------------------------------------------------------
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define HH_ARGPARSE_SHORT_PREFIX
#define HH_ARGPARSE_IMPLEMENTATION
#include "hh_argparse.h"

#define HH_DARRAY_SHORT_PREFIX
#define HH_DARRAY_IMPLEMENTATION
#include "hh_darray.h"

//-----------------------------------------------------------------------------
// Enums
typedef enum{
	GATE_INPUT = 0xFF0000FF,
	NOT_GATE = 0x000FFFF,
	DIODE = 0x00FF00FF
}gate_type_e;

typedef enum{
	LOW = 0,
	HIGH
}wire_state_e;

const int direction_x[4] = {1, 0, -1, 0};
const int direction_y[4] = {0, 1, 0, -1};
typedef enum{
	RIGHT = 0,
	DOWN,
	LEFT,
	UP,
}gate_direction_e;

typedef enum{
	SPACE = 0x000000FF,
	WIRE_WHITE = 0xFFFFFFFF,
	WIRE_MAGENTA = 0xFF00FFFF,
	WIRE_YELLOW = 0xFFFF00FF,
	WIRE_CYAN = 0x00FFFFFF,
	WIRE_CROSSING = 0xFF0000FF 
}wire_color_e;
//-----------------------------------------------------------------------------
// Structs
typedef struct{
	size_t x, y;
	size_t input_wire_id;
	size_t output_wire_id;
	gate_type_e type;
	gate_direction_e direction;
}gate_t;

typedef struct{
	wire_state_e state;
	wire_state_e state_buf;
	bool touchable;
	hh_darray_t pixels; // sizeof(sizet_t)*2
	wire_color_e color;
}wire_t;

//-----------------------------------------------------------------------------
// Global Variables
hh_darray_t global_gates;
hh_darray_t global_wires;

//-----------------------------------------------------------------------------
// Function Prototypes
void extract_gates_from_image(Image* img);
void extract_wires_from_image(Image* img);
void attack_gate_to_wires();
void step_simulation();
wire_color_e get_wire_color(Color color);
Color lower_color(Color color);
size_t get_wire_from_pixel(size_t x, size_t y);
size_t get_gate_from_pixel(size_t x, size_t y);

//-----------------------------------------------------------------------------

int main(int argc, char**argv){
	//...
	hh_argparse_t* argpar = hap_init(argc, argv);
	//-----------------------------------------------------------------------------
	// Get optional arguments
	int render_scale = 1;
	int simulation_rate = 60;
	// Render Scale
	if(hap_check_op_short_or_long(argpar, 's', "scale")){
		char *rc_str = hap_get_op_short_or_long(argpar, 's', "scale");
		render_scale = atoi(rc_str);
	}
	// Simulation Rate
	if(hap_check_op_short_or_long(argpar, 'r', "rate")){
		char *sr_str = hap_get_op_short_or_long(argpar, 'r', "rate");
		simulation_rate = atoi(sr_str);
	}
	// Help Message
	if(hap_check_op_short_or_long(argpar, 'h', "help")){
		printf("BitWidgets - A simple logic gate simulator using images as circuit blueprints.\n");
		printf("Usage: bitwidgets [options] <circuit_image_path>\n\n");
		printf("Options:\n");
		printf("  -s, --scale <num>      Set render scale (default: 1)\n");
		printf("  -h, --help             Show this help message\n");
		return 0;
	}
	//-----------------------------------------------------------------------------
	// Load Circuit Image
	if(argc < 2){
		printf("Error: No circuit image file provided.\n");
		printf("Use -h or --help for usage information.\n");
		return -1;
	}
	// Initilize Raylib
	SetConfigFlags(FLAG_WINDOW_UNDECORATED | 
				   FLAG_WINDOW_TRANSPARENT | 
				   FLAG_WINDOW_TOPMOST);
	InitWindow(100, 100, "BitWidgets");
	SetTargetFPS(60);
	//...
	hda_init(&global_gates, sizeof(gate_t));
	hda_init(&global_wires, sizeof(wire_t));
	//...
	Image circuit_img = LoadImage(hap_get_positional(argpar, 0));
	if(circuit_img.data == NULL){
		printf("Error: Failed to load circuit image file: %s\n", hap_get_positional(argpar, 0));
		return -1;
	}
	printf("[BITWIDGETS] Extracting gates...\n");
	extract_gates_from_image(&circuit_img);
	//...
	printf("[BITWIDGETS] Extracting Wires...\n");
	extract_wires_from_image(&circuit_img);
	//...
	printf("[BITWIDGETS] Attaching gates and wires...\n");
	attack_gate_to_wires();
	printf("[BITWIDGETS] Ready!\n");
	//-----------------------------------------------------------------------------
	//Preprocess image
    ImageColorReplace(&circuit_img, (Color){0, 0, 0, 255}, 
    							    (Color){0, 0, 0, 0});
	ImageResizeNN(&circuit_img, circuit_img.width*render_scale,
							   circuit_img.height*render_scale);
	//-----------------------------------------------------------------------------
	// Load as Texture
	Texture2D circuit_2d = LoadTextureFromImage(circuit_img);
	//...
	SetWindowSize(circuit_2d.width, circuit_2d.height);
	//-----------------------------------------------------------------------------
	// Main Loop
    while (!WindowShouldClose())
    {	
    	if(IsMouseButtonPressed(0)){
    		int mouse_x = GetMouseX() / render_scale;
    		int mouse_y = GetMouseY() / render_scale;
    		size_t wire_id = get_wire_from_pixel(mouse_x, mouse_y);
    		if(wire_id != (size_t)-1){
    			printf("wire id: %ld\n", wire_id);
	    		wire_t* wire = hda_get_reference(&global_wires, wire_id);
					if(wire->touchable) wire->state = !wire->state;
    		}
    	}
    	
			BeginDrawing();
			//---------------------------------------------------------------------
			// Simulation Steps
			static long sim_accumulator = 0;
			double start_time = GetTime();
			while(sim_accumulator < GetTime() * simulation_rate){
				step_simulation();
				sim_accumulator++;
				if(GetTime() - start_time > 1){
					printf("Warning: Simulation is lagging behind real time!\n");
					break;
				}
			}
			// Report Performance
			static double last_report_time = 0;
			if(GetTime() - last_report_time >= 1.0f){
				printf("[BITWIDGETS] FPS: %d| Gates: %ld | Wires: %ld\n", 
							GetFPS(),  hda_get_item_fill(&global_gates), hda_get_item_fill(&global_wires));
				last_report_time = GetTime();
			}
			//---------------------------------------------------------------------
			ClearBackground((Color){0, 0, 0, 0});
		
			// Draw Gates
			for(size_t i = 0; i < hda_get_item_fill(&global_gates); i++){
				gate_t* gate = hda_get_reference(&global_gates, i);
				DrawRectangle(gate->x*render_scale, gate->y*render_scale, 
								render_scale, render_scale, GetColor(GATE_INPUT));
				DrawRectangle((gate->x + direction_x[gate->direction])*render_scale, 
								(gate->y + direction_y[gate->direction])*render_scale, 
								render_scale, render_scale, GetColor(gate->type));
			}

			// Draw Wires
			for(size_t i = 0; i < hda_get_item_fill(&global_wires); i++){
				wire_t* wire = hda_get_reference(&global_wires, i);
				for(size_t p = 0; p < hda_get_item_fill(&wire->pixels); p++){
					struct {size_t x, y;} pix;
					hda_get(&wire->pixels, p, &pix);
					if(wire->state){
						DrawRectangle(pix.x*render_scale, pix.y*render_scale, 
											render_scale, render_scale, GetColor(wire->color));
					}
					else{
						DrawRectangle(pix.x*render_scale, pix.y*render_scale, 
											render_scale, render_scale, lower_color(GetColor(wire->color)));
					}
				}
			}
       	
			//---------------------------------------------------------------------
			EndDrawing();
    }

	//---------------------------------------------------------------------------
    CloseWindow();
	return 0;
}

//-----------------------------------------------------------------------------
void extract_gates_from_image(Image* img){
	for(size_t y = 0; y < (unsigned int)img->height; y++){
		for(size_t x = 0; x < (unsigned int)img->width; x++){
			Color color = GetImageColor(*img, x, y);
			if(ColorToInt(color) == (int)GATE_INPUT){
				for(int n = 0; n < 4; n++){
					Color n_color = GetImageColor(*img, x + direction_x[n], 
													   y + direction_y[n]);
					if(ColorToInt(n_color) == (int)NOT_GATE){
						hda_append(&global_gates, 0);
						gate_t* new_gate = hda_get_end_reference(&global_gates);
						new_gate->x = x; 
						new_gate->y = y;
						new_gate->type = NOT_GATE;
						new_gate->direction = n;
					}
					if(ColorToInt(n_color) == (int)DIODE){
						hda_append(&global_gates, 0);
						gate_t* new_gate = hda_get_end_reference(&global_gates);
						new_gate->x = x; 
						new_gate->y = y;
						new_gate->type = DIODE;
						new_gate->direction = n;
					}
					
				}

			}	
		}
	}
	// Remove Gates
	for(size_t i = 0; i < hda_get_item_fill(&global_gates); i++){
		gate_t* gate = hda_get_reference(&global_gates, i);
		ImageDrawPixel(img, gate->x, gate->y, (Color){0, 0, 0, 0});
		ImageDrawPixel(img, gate->x + direction_x[gate->direction], 
							gate->y + direction_y[gate->direction], 
							(Color){0, 0, 0, 0});
	}
}
//-----------------------------------------------------------------------------
void extract_wires_from_image(Image* img){
	for(size_t y = 0; y < (unsigned int)img->height; y++){
		for(size_t x = 0; x < (unsigned int)img->width; x++){
			Color color = GetImageColor(*img, x, y);
			wire_color_e temp_color = get_wire_color(color);
			if(temp_color == SPACE) continue;	
			if(temp_color == WIRE_CROSSING) continue;	
			//...
			hda_append(&global_wires, 0);
			wire_t* new_wire = hda_get_end_reference(&global_wires);
			hda_init(&new_wire->pixels, sizeof(size_t)*2);
			new_wire->color = temp_color;
			new_wire->touchable = true;
			// Flood fill wire
			hh_darray_t checker; hda_init(&checker, sizeof(size_t)*2);
			hh_darray_t skipper; hda_init(&skipper, 1);
			hda_append(&checker, (size_t[]){x, y});
			hda_append(&skipper, 0);
			bool skip_val = 1;					
			while(0 < hda_get_item_fill(&checker)){
				struct {size_t x; size_t y;} pix;
				bool skip;
				hda_pop(&checker, 0, &pix);
				hda_pop(&skipper, 0, &skip);
				if(!skip){
					hda_append(&new_wire->pixels, &pix);
					ImageDrawPixel(img, pix.x, pix.y, (Color){0, 0, 0, 0});
				}
				// Check neighbors
				if(pix.x < (unsigned int)img->width-1){
					if(get_wire_color(GetImageColor(*img, pix.x + 1, pix.y)) == new_wire->color){
						if(hda_append_no_dupe(&checker, (size_t[]){pix.x + 1, pix.y})){
							hda_append(&skipper, 0);
						}
					}
					if(!skip){
						if(get_wire_color(GetImageColor(*img, pix.x + 1, pix.y)) == WIRE_CROSSING){	
							if(get_wire_color(GetImageColor(*img, pix.x + 2, pix.y)) == new_wire->color){
								if(hda_append_no_dupe(&checker, (size_t[]){pix.x + 2, pix.y})){
									hda_append(&skipper, 0);
								}
							}
						}
						if(get_gate_from_pixel(pix.x + 1, pix.y) != (size_t)-1){
							if(hda_append_no_dupe(&checker, (size_t[]){pix.x + 1, pix.y})){
								hda_append(&skipper, &skip_val);						
							}
						}
					}
				}
				if(pix.x > 0){
					if(get_wire_color(GetImageColor(*img, pix.x - 1, pix.y)) == new_wire->color){
						if(hda_append_no_dupe(&checker, (size_t[]){pix.x - 1, pix.y})){
							hda_append(&skipper, 0);
						}
					}
					if(!skip){
						if(get_wire_color(GetImageColor(*img, pix.x - 1, pix.y)) == WIRE_CROSSING){	
							if(get_wire_color(GetImageColor(*img, pix.x - 2, pix.y)) == new_wire->color){
								if(hda_append_no_dupe(&checker, (size_t[]){pix.x - 2, pix.y})){
									hda_append(&skipper, 0);
								}
							}
						}
						if(get_gate_from_pixel(pix.x - 1, pix.y) != (size_t)-1){
							if(hda_append_no_dupe(&checker, (size_t[]){pix.x - 1, pix.y})){
								hda_append(&skipper, &skip_val);						
							}
						}
					}
				}
				if(pix.y < (unsigned int)img->height-1){
					if(get_wire_color(GetImageColor(*img, pix.x, pix.y + 1)) == new_wire->color){
						if(hda_append_no_dupe(&checker, (size_t[]){pix.x, pix.y + 1})){
							hda_append(&skipper, 0);
						}
					}
					if(!skip){
						if(get_wire_color(GetImageColor(*img, pix.x, pix.y + 1)) == WIRE_CROSSING){	
							if(get_wire_color(GetImageColor(*img, pix.x, pix.y + 2)) == new_wire->color){
								if(hda_append_no_dupe(&checker, (size_t[]){pix.x, pix.y + 2})){
									hda_append(&skipper, 0);
								}
							}
						}
						if(get_gate_from_pixel(pix.x, pix.y + 1) != (size_t)-1){
							if(hda_append_no_dupe(&checker, (size_t[]){pix.x, pix.y + 1})){
								hda_append(&skipper, &skip_val);						
							}
						}
					}
				}
				if(pix.y > 0){
					if(get_wire_color(GetImageColor(*img, pix.x, pix.y - 1)) == new_wire->color){
						if(hda_append_no_dupe(&checker, (size_t[]){pix.x, pix.y - 1})){
							hda_append(&skipper, 0);
						}
					}
					if(!skip){
						if(get_wire_color(GetImageColor(*img, pix.x, pix.y - 1)) == WIRE_CROSSING){	
							if(get_wire_color(GetImageColor(*img, pix.x, pix.y - 2)) == new_wire->color){
								if(hda_append_no_dupe(&checker, (size_t[]){pix.x, pix.y - 2})){
									hda_append(&skipper, 0);
								}
							}
						}
						if(get_gate_from_pixel(pix.x, pix.y - 1) != (size_t)-1){
							if(hda_append_no_dupe(&checker, (size_t[]){pix.x, pix.y - 1})){
								hda_append(&skipper, &skip_val);						
							}
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
wire_color_e get_wire_color(Color color){
	if(color.r > 0 && color.g > 0 && color.b > 0) 
		return WIRE_WHITE;
	else if(color.r > 0 && color.g == 0 && color.b > 0) 
		return WIRE_MAGENTA;
	else if(color.r > 0 && color.g > 0 && color.b == 0) 
		return WIRE_YELLOW;
	else if(color.r == 0 && color.g > 0 && color.b > 0) 
		return WIRE_CYAN;
	else if(color.r > 0 && color.g == 0 && color.b == 0) 
		return WIRE_CROSSING;
	else
		return SPACE;
}

//-----------------------------------------------------------------------------
Color lower_color(Color color){
	return (Color){color.r/2, color.g/2, color.b/2, color.a};
}

//-----------------------------------------------------------------------------
void attack_gate_to_wires(){
	for(size_t i = 0; i < hda_get_item_fill(&global_gates); i++){
		gate_t* gate = hda_get_reference(&global_gates, i);
		for(int d = 0; d < 4; d++){
			size_t input_id = get_wire_from_pixel(gate->x + direction_x[d], gate->y + direction_y[d]);
			if(input_id != (size_t)-1){
				gate->input_wire_id = input_id;
				break;
			}
		}
		for(int d = 0; d < 4; d++){
			size_t output_id = get_wire_from_pixel(gate->x + direction_x[d] + direction_x[gate->direction], 
												   gate->y + direction_y[d] + direction_y[gate->direction]);
			if(output_id != (size_t)-1){
				wire_t* wire = hda_get_reference(&global_wires, output_id);
				wire->touchable = false;
				gate->output_wire_id = output_id;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
void step_simulation(){
	//Clear buffers
	for(size_t i = 0; i < hda_get_item_fill(&global_wires); i++){
		wire_t* wire = hda_get_reference(&global_wires, i);
		if(!wire->touchable)
			wire->state_buf = 0;
	}
	// Evaluate Outputs of gates
	for(size_t i = 0; i < hda_get_item_fill(&global_gates); i++){
		gate_t* gate = hda_get_reference(&global_gates, i);
		wire_t* wire_out = hda_get_reference(&global_wires, gate->output_wire_id);
		wire_t* wire_in = hda_get_reference(&global_wires, gate->input_wire_id);
		if(gate->type == NOT_GATE){
			if(!wire_in->state) wire_out->state_buf = 1;
		}
		else if(gate->type == DIODE){
			if(wire_in->state) wire_out->state_buf = 1;
		}
	}
	// Swap Buffers
	for(size_t i = 0; i < hda_get_item_fill(&global_wires); i++){
		wire_t* wire = hda_get_reference(&global_wires, i);
		if(!wire->touchable)
			wire->state = wire->state_buf;
	}
}

//-----------------------------------------------------------------------------
size_t get_wire_from_pixel(size_t x, size_t y){
	for(size_t i = 0; i < hda_get_item_fill(&global_wires); i++){
		wire_t* wire = hda_get_reference(&global_wires, i);
		for(size_t p = 0; p < hda_get_item_fill(&wire->pixels); p++){
			struct {size_t x, y;} pix;
			hda_get(&wire->pixels, p, &pix);
			if(pix.x == x && pix.y == y) return i;
		}	
	}
	return -1;
}

//-----------------------------------------------------------------------------
size_t get_gate_from_pixel(size_t x, size_t y){
	for(size_t i = 0; i < hda_get_item_fill(&global_gates); i++){
		gate_t* gate = hda_get_reference(&global_gates, i);
		if(gate->x == x && gate->y == y) return i;
		if(gate->x + direction_x[gate->direction] == x && gate->y + direction_y[gate->direction] == y) return i;
	}
	return -1;
}
