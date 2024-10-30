#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <raylib.h>

#define CELL_SIZE 1024
#define MEMORY_SIZE 64
#define STORAGE_SIZE 64
#define MAX_EVENTS 8

typedef struct State State;
typedef struct RenderInfo RenderInfo;
typedef struct UpdateEvent UpdateEvent;
typedef enum UpdateEventType UpdateEventType;
typedef enum EasingFunction EasingFunction;

// Rendering Constants
#define SLIDE_IN_TIME .5 // Seconds
#define X_PADDING 40

struct RenderInfo {
	Color background;
	Color font_color;
	float start_time;
	int header_gap;
	double register_height;
	double memory_height;
	double storage_height;
	int register_cell_width;
	int register_cell_height;
	int cell_height;
	int cell_width;
	int cell_gap;
	Color cell_color;
	int scroll_speed;
	UpdateEvent* update_events[MAX_EVENTS]; 
	Rectangle pointer; // Program Counter
};

enum UpdateEventType {
	REGISTER_SEEK,
	MEMORY_SEEK,
	STORAGE_SEEK
};

enum EasingFunction {
	LINEAR,
	IN_N_OUT
};

struct UpdateEvent {
	UpdateEventType type;
	int which; // R1 == 1, Memory cell 1 == 1
	EasingFunction easing;
	double percent; // 0-1 completeness
	float duration; // Seconds
	float start_val;
	float end_val;
};

void UpdateEventPrint(UpdateEvent* e) {
	printf("{ .%%=%lf, .duration=%f, .start=%f, .end=%f }\n", e->percent, e->duration, e->start_val, e->end_val);
}


struct State {
	RenderInfo render_info;
	int registers[10];
	char* memory[MEMORY_SIZE];
	char* storage[STORAGE_SIZE];
};

static State* s = NULL;

uint16_t LoadFileIntoMemory(char* filename, char* memory[]);
void plug_init(char* filename);
State* plug_pre_reload();
void plug_update();
void StartVisualisation();
void RenderEvents();
void RenderFrame();
void RenderRegister(int i);
int MinInt(int a, int b) {
	return a < b ? a: b;
}
double MinDouble(double a, double b) {
	return a < b ? a: b;
}
int MaxInt(int a, int b) {
	return a > b ? a: b;
}
int BoundInt(int v, int lower, int upper) {
	return MaxInt(MinInt(v, upper), lower);
}
float ParametricBlend(float t) {
	// Source: https://stackoverflow.com/questions/13462001/ease-in-and-ease-out-animation-formula
    float sqr = t * t;
    return sqr / (2.0f * (sqr - t) + 1.0f);
}

float UpdateEventGetCurrentValue(UpdateEvent* update_event) {
	float percent = update_event->percent;
	switch (update_event->easing) {
		case LINEAR:
			break;
		case IN_N_OUT:
			percent = ParametricBlend(percent);
			break;
		default:
			fprintf(stderr, "Unexpected easing function %d\n", update_event->easing);
	}
	return update_event->start_val + ((update_event->end_val - update_event->start_val) * percent);
}

void UpdateEventUpdatePercent(UpdateEvent* update_event, float dt) {
	update_event->percent = MinDouble(dt / update_event->duration + update_event->percent, 1.0F);
}

uint16_t LoadFileIntoMemory(char* filename, char* memory[]) {
	FILE* file_p;
	if ((file_p = fopen(filename, "r")) < 0) {
		perror("Open file: ");
		exit(1);
	}

	uint16_t lines = 0;
	int n_read;
	size_t num_to_read;
	while (1) {
		char* linep = (char*) malloc(CELL_SIZE);
		n_read = getline(&linep, &num_to_read, file_p);
		if (n_read <= 0) {
			break;
		}
		memory[lines++] = linep;
	}
	return lines;
}

float GetMidPoint() {
	RenderInfo* render_info = &s->render_info;
	return GetScreenHeight() / 2 - render_info->cell_height + render_info->cell_height / 2;
}


void plug_init(char* filename) {
	RenderInfo render_info = {
		.background = GetColor(0x181818FF),
		.font_color = GetColor(0xFFFFFFFF),
		.start_time = 2.5,
		.header_gap = 100,
		.register_height = GetScreenHeight(),
		.memory_height = GetScreenHeight(),
		.storage_height = GetScreenHeight(),
		.register_cell_width = 160,
		.register_cell_height = 40,
		.cell_height = 65,
		.cell_width = 250,
		.cell_gap = 20,
		.cell_color = GetColor(0xcaccde),
		.scroll_speed = 4,
		.update_events = { NULL },
		.pointer = {
			.x = 0,
			.y = 0,
			.width = 270,
			.height = 85
		},
	};

	s = malloc(sizeof(State));
	assert(s != NULL);
	memset(s, 0, sizeof(State));
	s->render_info = render_info;
	s->render_info.pointer.y = GetMidPoint() - (s->render_info.pointer.height - s->render_info.cell_height) / 2;
	s->render_info.pointer.x = X_PADDING - (s->render_info.pointer.width - s->render_info.cell_width) / 2;
	LoadFileIntoMemory(filename, s->memory);

	StartVisualisation();
}

State* plug_pre_reload() {
	return s;
}

void plug_post_reload(void* state) {
	s = (State*)state;
}

// -------------
// Events
// -------------
void CreateRegisterSeekEvent(float y, EasingFunction easing) {
	// Only seeks to a cell, does not modify program counter
	RenderInfo* render_info = &s->render_info;
	UpdateEvent* event = (UpdateEvent*)malloc(sizeof(UpdateEvent));
	event->type = REGISTER_SEEK;
	event->which = -1; // Not used
	event->easing = easing;
	event->percent = 0;
	event->duration = SLIDE_IN_TIME; 
	event->start_val = render_info->register_height;
	event->end_val = y;
	for (int i = 0; i < MAX_EVENTS; i++) {
		if (render_info->update_events[i] == NULL) {
			render_info->update_events[i] = event;
			break;
		}
	}
}
void CreateMemorySeekEvent(uint16_t index, EasingFunction easing) {
	// Only seeks to a cell, does not modify program counter
	RenderInfo* render_info = &s->render_info;
	UpdateEvent* event = (UpdateEvent*)malloc(sizeof(UpdateEvent));
	event->type = MEMORY_SEEK;
	event->which = index;
	event->easing = easing;
	event->percent = 0;
	event->duration = SLIDE_IN_TIME; 
	event->start_val = render_info->memory_height;
	event->end_val = -render_info->cell_height*index - render_info->cell_gap*index + GetScreenHeight() / 2 - render_info->cell_height / 2;
	for (int i = 0; i < MAX_EVENTS; i++) {
		if (render_info->update_events[i] == NULL) {
			render_info->update_events[i] = event;
			break;
		}
	}
}

void CreateStorageSeekEvent(uint16_t index, EasingFunction easing) {
	// Only seeks to a cell, does not modify program counter
	RenderInfo* render_info = &s->render_info;
	UpdateEvent* event = (UpdateEvent*)malloc(sizeof(UpdateEvent));
	event->type = STORAGE_SEEK;
	event->which = index;
	event->easing = easing;
	event->percent = 0;
	event->duration = SLIDE_IN_TIME; 
	event->start_val = render_info->storage_height;
	event->end_val = -render_info->cell_height*index - render_info->cell_gap*index + GetScreenHeight() / 2 - render_info->cell_height / 2;
	for (int i = 0; i < MAX_EVENTS; i++) {
		if (render_info->update_events[i] == NULL) {
			render_info->update_events[i] = event;
			break;
		}
	}
}

void EventFree(UpdateEvent* update_event) {
	free(update_event);
}

void StartVisualisation() {
	RenderInfo* render_info = &s->render_info;
	InitWindow(800, 600, "Mini Asm");	
	RenderEvents();
	CreateMemorySeekEvent(0, IN_N_OUT);
	RenderEvents();
	CreateRegisterSeekEvent(render_info->header_gap, IN_N_OUT);
	RenderEvents();
	CreateStorageSeekEvent(0, IN_N_OUT);
	RenderEvents();
	CreateMemorySeekEvent(1, IN_N_OUT);
	RenderEvents();
	CreateMemorySeekEvent(2, IN_N_OUT);
	RenderEvents();
	
}

void plug_update() {
	// RenderInfo* render_info = &s->render_info;
	// int mouse_move = (int)(GetMouseWheelMove()*render_info->scroll_speed);
	// render_info->memory_height -= mouse_move;
	// render_info->memory_height = BoundInt(
	//	render_info->memory_height, 
	//	-(render_info->cell_height*MEMORY_SIZE + render_info->cell_gap*MEMORY_SIZE) + GetScreenHeight()/2, 
	//	GetMidPoint()
	// );
	// render_info->pointer.y = render_info->memory_height - (render_info->pointer.height - render_info->cell_height) / 2;
	RenderEvents();
}

void RenderEvents() {
	RenderFrame();
	RenderInfo* render_info = &s->render_info;
	UpdateEvent** update_events = render_info->update_events; 
	while (!WindowShouldClose()) {
		bool has_events = false;
		float dt = GetFrameTime();
		for (int i = 0; i < MAX_EVENTS; i++) {
			UpdateEvent* update_event = update_events[i];
			if (update_event != NULL) {
				has_events = true;
			} else {
				continue;
			}
			UpdateEventUpdatePercent(update_event, dt);

			switch (update_event->type) {
				case REGISTER_SEEK:
					render_info->register_height = UpdateEventGetCurrentValue(update_event);
					break;
				case MEMORY_SEEK:
					render_info->memory_height = UpdateEventGetCurrentValue(update_event);
					break;
				case STORAGE_SEEK:
					render_info->storage_height = UpdateEventGetCurrentValue(update_event);
					break;
				default:
					fprintf(stderr, "Unexpected update event type %d\n", update_event->type);
			}
			if (update_event->percent == 1) {
				update_events[i] = NULL;
				EventFree(update_event);
			}
		}
		RenderFrame();
		if (!has_events) {
			break;
		}
	}
}

void RenderFrame() {
	RenderInfo* render_info = &s->render_info;
	char** memory = s->memory;
	char** storage = s->storage;

	BeginDrawing();
	ClearBackground(render_info->background);
	// Memory
	DrawText("Memory", X_PADDING, render_info->cell_gap, 24, render_info->font_color);
	for (int i = 0; i < MEMORY_SIZE; i++) {
		int y = render_info->memory_height + render_info->cell_height*i + render_info->cell_gap*i;
		DrawRectangle(
			X_PADDING,
			y,
			render_info->cell_width,
			render_info->cell_height,
			render_info->cell_color
		);
		char* msg = NULL;
		asprintf(&msg, "Ox%x: %s", i*4, memory[i]);
		DrawText(msg, X_PADDING, y+render_info->cell_height/2, 12, render_info->font_color);
	}
	DrawRectangleLinesEx(render_info->pointer, 2.0F, BLUE); 
		
	// Registers
	float textWidth = MeasureTextEx(GetFontDefault(), "Registers", 24, 1).x;
	Vector2 position = { .x=GetScreenWidth()/2 - textWidth/2, .y=render_info->cell_gap };
	DrawTextEx(GetFontDefault(), "registers", position, 24, 1, render_info->font_color);
	for (int i = 0; i < 10; i++) {
		RenderRegister(i);
	}
	

	// Storage
	textWidth = MeasureTextEx(GetFontDefault(), "Storage", 24, 1).x;
	DrawText("Storage", GetScreenWidth() - X_PADDING - textWidth, render_info->cell_gap, 24, render_info->font_color);
	for (int i = 0; i < STORAGE_SIZE; i++) {
		int y = render_info->storage_height + render_info->cell_height*i + render_info->cell_gap*i;
		DrawRectangle(
			GetScreenWidth() - X_PADDING - render_info->cell_width,
			y,
			render_info->cell_width,
			render_info->cell_height,
			render_info->cell_color
		);
		char* msg = NULL;
		asprintf(&msg, "Ox%x: %s", i*4, storage[i]);
		DrawText(msg, GetScreenWidth() - X_PADDING - render_info->cell_width, y+render_info->cell_height/2, 12, render_info->font_color);
	}
	
	EndDrawing();
}

void RenderRegister(int i) {
	RenderInfo* render_info = &s->render_info;
	int x = GetScreenWidth() / 2 - (render_info->register_cell_width/2);
	int y = render_info->register_height + render_info->register_cell_height* 1.15 * i;
	DrawRectangle(
		x,
		y,
		render_info->register_cell_width,
		render_info->register_cell_height,
		render_info->cell_color
	);
	char* msg = NULL;
	asprintf(&msg, "R%d: %d", i, s->registers[i]);
	if (i == 0) {
		free(msg);
		asprintf(&msg, "PC: %d", s->registers[0]);
	}
	DrawText(msg, x, y+render_info->register_cell_height/2, 12, render_info->font_color);
}
