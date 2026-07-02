#ifndef ITS_A_COMPUTER_H_
#define ITS_A_COMPUTER_H_

/* fundamental functions */
typedef struct { float r, g, b, a; } Color;
void setup(void);                       // function that gets called once at the beginning of each script
void draw(void);                        // function that gets called every frame
void set_background_color(Color color); // sets the global background color
void set_target_fps(int fps);           // sets the target fps, perhaps there is a minimum set for the VR app itself
float get_delta_time(void);             // gets the delta time
double get_time(void);                  // elapsed time since the start of the script
float get_fps(void);                    // get current fps

/* logging */
void show_debug_terminal(void);                      // make the debug terminal visible
void log_message(const char *message, ...);          // log message to debug terminal
void set_debug_terminal_size(int width, int height); // set terminal window size

/* file io */
typedef struct { char *path, *data; int count; } File;
bool read_file(const char *file_path, File *file); // read file from path
bool write_file(const char *file_path, File file); // write file to path
bool file_exists(const char *file_path);           // returns true if file exists

/* keyboard input */
bool is_key_pressed(int key); // check if key has been pressed once
bool is_key_down(int key);    // check if key is currently down

/* hand tracking */
bool is_pinched(int hand);                         // check if hand is pinched once (thumb + index)
bool is_pinching(int hand)                         // check if hand is pinching
Matrix get_bone_matrix(int bone);                  // get the 4x4 matrix describing the bone
Vector3 get_bone_position(int bone);               // position of the bone in 3D space
bool are_bones_overlapping(int bone1, int bone2);  // are two bones overlapping based on their collision sphere
void set_bone_collision_sphere_radius(int radius); // set the bones collision sphere radius
void show_bone_collsion_sphere(int bone);          // make the bone collision sphere visible

/* controller */
Matrix get_grip_pose_matrix(int hand); // get the controller grip pose matrix
Matrix get_aim_pose_matrix(int hand);  // get the controller aim pose matrix
bool is_button_pressed(int btn);       // check if controller button was pressed once
bool is_button_down(int btn);          // check if controller button is down
float get_trigger(int trigger);        // get the controller trigger value
Vector2 get_joy(int hand);             // get the joystick values (x, y)

/* head tracking*/
Matrix get_head_matrix(void);          // get the head matrix
Vector3 get_head_position(void);       // get the head position in 3D space
Vector3 get_head_look_direction(void); // get a normalized vector for the head

/* transformations */
void push_matrix(void);                     // push a matrix to the global matrix stack
void pop_matrix(void);                      // pop a matrix from the global matrix stack
void cat_matrix(Matrix m);                  // concatenate a matrix with the matrix on the top of stack
void translate(float x, float y, float z);  // apply translation to the matrix on the top of stack
void translate_v(Vector3 t);                // apply translation to the matrix on the top of stack
void rotate_x(float radians);               // rotate about the x-axis radians
void rotate_y(float radians);               // rotate about the y-axis radians
void rotate_z(float radians);               // rotate about the z-axis radians
void rotate_x_degrees(float degrees);       // rotate about the x-axis degrees 
void rotate_y_degrees(float degrees);       // rotate about the y-axis degrees 
void rotate_z_degrees(float degrees);       // rotate about the z-axis degrees 
void scale_uniform(float s);                // apply uniform scale to the matrix on the top of stack
void scale(float x, float y, float z);      // apply scale to the matrix on the top of stack
void scale_v(Vector3 s);                    // apply scale to the matrix on the top of stack

/* text */
Label create_label(int width, int height);                                                     // create a 3D label that text can write to
void write_to_label(Label *label, const char *text, int x, int x, int font_size, Color color); // write to a text label
void draw_text_label(Lable label, bool billboard);                                             // draw the text label (billboard: always face look direction)

/* basic 3D */
void draw_line(Vector3 begin, Vector3 end, Color color);                              // draw a line in 3D world space
void draw_cube(float width, float height, float length, Color color);                 // draw cube
void draw_cube_v(Vector3 size, Color color);                                          // draw cube (Vector version)
void draw_cube_wires(float width, float height, float length, Color color);           // draw cube wires
void draw_cube_wires_v(Vector3 size, Color color);                                    // Draw cube wires (Vector version)
void draw_sphere(float radius, Color color);                                          // Draw sphere
void draw_sphere_wires(float radius, Color color);                                    // Draw sphere wires
void draw_cylinder(float radius_top, float radius_bottom, float height, Color color); // draw a cylinder/cone
void draw_capsule(Vector3 begin, Vector3 end, float radius, Color color);             // draw a capsule from start to end position
void draw_capsule_wires(Vector3 begin, Vector3 end, float radius, Color color);       // draw a capsule from start to end position wireframe
void draw_plane(Vector2 size, Color color);                                           // draw a plane in xz-plane
void draw_grid(int slices, float spacing);                                            // draw a grid 

/* 3D models */
Model load_model(const char *file_name);      // load a 3D model
void unload_model(Model m);                   // unload 3D model
void draw_model(Model m, Color tint);         // draw 3D model with tint
void draw_model_wires(Model m, Color tint);   // draw 3D wireframe model
void draw_model_points(Model m, Color tint);  // draw 3D model as points
Bounding_Box get_model_bounding_box(Model m); // get the bounding box of the 3D model
void draw_bounding_box(Bounding_Box b);       // draw the bounding box

/* animations */
Animation *load_model_animations(const char *file_name, int *anim_count); // Load model animations from file
void update_animation(Model model, Animation anim, int frame);            // Update model animation pose (CPU)
void update_animation_bones(Model model, Animation anim, int frame);      // Update model animation mesh bone matrices (GPU skinning)
void unload_animation(Animation anim);                                    // Unload animation data
void unload_animations(Animation *animations, int anim_count);            // Unload animation array data

/* collision */
bool check_collision_spheres(Vector3 center1, float radius1, Vector3 center2, float radius2); // sphere-sphere collision
bool check_collision_boxes(Bounding_Box bb1, Bounding_Box bb2);                               // oriented bouding box collision
bool check_collision_box_sphere(Bounding_Box bb1, Vector3 center, float radius);              // bounding-box-sphere collision

/* audio */
Sound load_sound(const char *file_name); // load sound from file
void unload_sound(Sound s);              // unload the sound
void play_sound(Sound s);                // play the sound
void pause_sound(Sound s);               // pause the sound
void restart_sound(Sound s);             // restart sound to beginning
void is_sound_playing(Sound s);          // check if sound is currently playing
void set_sound_volume(Sound s);          // set the volume of this sound

#endif // ITS_A_COMPUTER_H_
