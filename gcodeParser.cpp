// This is a GCODE parser. It will read a gcode file and help opengl draw it in 3D
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>

#define PI 3.14159265

#define FRAME_TIMEOUT 20
#define MAX_LINE_INPUT 999
#define MIN_EXTRUSION 1
#define MAX_DRAW_LINE_THICKNESS 0.1
#define MIN_EXTRUDE_DIFF 0.01
#define MAX_EXTRUDE_DIFF 10

#define MAX_KEYBOARD_COMBINATION 7
#define SCROLL_SCALE_RATE 0.1
#define PAN_RATE_X 0.0001
#define PAN_RATE_Y 0.0001
#define PAN_RATE_Z 0.0001
#define ROTATE_RATE 0.4

#define COMMAND_LINEARMOVE "G1"



std::string * split( char * input, char delim );
int string_to_int( std::string str );
float string_to_float( std::string str );
std::string int_to_string( int num );
float * parse_move_command( std::string * input );
std::string * parse_move_command_giveString(std::string * input);

int WINDOW_WIDTH;
int WINDOW_HEIGHT;


// these are the values that allow panning.
float GLOBAL_ROTATE_X;
float GLOBAL_ROTATE_Y;
float GLOBAL_ROTATE_Z;

float GLOBAL_PHI;
float GLOBAL_THETA;


float GLOBAL_TRANSFORM_X;
float GLOBAL_TRANSFORM_Y;
float GLOBAL_TRANSFORM_Z;


float OBJECT_MIDDLE_X;
float OBJECT_MIDDLE_Y;
float OBJECT_MIDDLE_Z;

float OBJECT_SCALING;

int GLOBAL_GCODE_START;
int GLOBAL_GCODE_END;

//****************************************************************************************************************//

class EXTRUSION{
public:
	float x_start;
	float y_start;
	float z_start;
	float x_end;
	float y_end;
	float z_end;

	float thickness;

	EXTRUSION(  ){
		x_start = 0;
		y_start = 0;
		z_start = 0;
		x_end = 1;
		y_end = 1;
		z_end = 1;
		thickness = 0.1;
	}

	EXTRUSION( float _xs, float _ys, float _zs, float _xe, float _ye, float _ze ){
		x_start = _xs;
		y_start = _ys;
		z_start = _zs;

		x_end = _xe;
		y_end = _ye;
		z_end = _ze;
		thickness = 0.1;
	}

	EXTRUSION( float _xs, float _ys, float _zs, float _xe, float _ye, float _ze, float _thickness ){
		x_start = _xs;
		y_start = _ys;
		z_start = _zs;

		x_end = _xe;
		y_end = _ye;
		z_end = _ze;
		thickness = _thickness;
	}

	// render without regard of the scaling variable
	void render(){
		glLineWidth( thickness );

		glBegin( GL_LINES );
			glVertex3f( x_start, 	y_start,	z_start );
			glVertex3f( x_end, 		y_end ,		z_end );
		glEnd();
	}

	// render with regard to scaling. But no color.
	void render( float _scaling ){
		glLineWidth( thickness );

		glBegin( GL_LINES );
			glVertex3f( x_start * _scaling, 		y_start * _scaling,		z_start *_scaling );
			glVertex3f( x_end * _scaling, 		y_end  * _scaling,		z_end * _scaling );
		glEnd();
	}


	// render with regard to SCALING and Z LEVEL
	void render( float _scaling, float max_z ){
		glLineWidth( thickness );
		float color_ratio = (z_end * _scaling)/max_z;

		glColor3f( 0.0, color_ratio , 1 - color_ratio );
		glBegin( GL_LINES );
			glVertex3f( x_start * _scaling, 		y_start * _scaling,		z_start *_scaling );
			glVertex3f( x_end * _scaling, 		y_end  * _scaling,		z_end * _scaling );
		glEnd();
	}

};



//****************************************************************************************************************//

class GCODE_OBJECT{
public:
	// the array that will hold each gcode extrusion
	EXTRUSION * data;
	long int data_len;

	// the array that will hold the extrusions with positions from -1 to 1
	EXTRUSION * data_normalized;
	int data_normalized_len;

	float * angle_phi;
	float * angle_theta;
	float * angle_z;

	float * look_at_x;
	float * look_at_y;
	float * look_at_z;

	float * scaling;

	bool rotating;
	bool panning;
	int current_x;
	int current_y;

	FILE * file_vertex;	
	FILE * file_extrusions;
	FILE * file_vertex_normalized;

	char * path_file_vertex;
	char * path_file_extrusions;
	char * path_file_vertex_normalized;

	bool file_rendering;

	float MAX_X;
	float MAX_Y;
	float MAX_Z;

	int * render_start;
	int * render_end;

	GCODE_OBJECT(){
		// start with data and normalized data empty.
		data = new EXTRUSION[0];
		data_len = 0;

		data_normalized = new EXTRUSION[0];
		data_normalized_len = 0;
		rotating = false;
		panning = false;
		scaling = new float[1];
		*scaling = 1;
		file_rendering = false;
		render_start = new int[0];
		render_end = new int[0];
	}

	// if you want to add an extrusion object
	void add_to_data( EXTRUSION e ){
		// create a tempory array and copy contents to it
		EXTRUSION * temp = new EXTRUSION[ data_len + 1 ];
		std::copy( data, data + data_len, temp );
		// resize data and store new element
		delete[] data;
		data = temp;
		data[ data_len ] = e;
		data_len += 1;
	}

	// if you wanna add one by coordinates
	void add_to_data( float _x1, float _y1, float _z1, float _x2, float _y2, float _z2  ){
		// create a tempory array and copy contents to it
		EXTRUSION * temp = new EXTRUSION[ data_len + 1 ];
		std::copy( data, data + data_len, temp );
		// resize data and store new element
		delete[] data;
		data = temp;
		data[ data_len ] = EXTRUSION( _x1, _y1, _z1, _x2, _y2, _z2 );
		data_len += 1;
	}


	// if you wanna add one by coordinates WITH custom thickness
	void add_to_data( float _x1, float _y1, float _z1, float _x2, float _y2, float _z2, float _thick  ){
		// create a tempory array and copy contents to it
		EXTRUSION * temp = new EXTRUSION[ data_len + 1 ];
		std::copy( data, data + data_len, temp );
		// resize data and store new element
		delete[] data;
		data = temp;
		data[ data_len ] = EXTRUSION( _x1, _y1, _z1, _x2, _y2, _z2, _thick );
		data_len += 1;
	}

	void reset_data_size( int a ){
		EXTRUSION * temp = new EXTRUSION[ a ];
		delete[] data;
		data = temp;
		data_len = a;
	}

	void set_data( int i, float _x1, float _y1, float _z1, float _x2, float _y2, float _z2, float _thick ){
		data[i] = EXTRUSION( _x1, _y1, _z1, _x2, _y2, _z2, _thick );
	}

	void add_to_data_normalized( EXTRUSION e ){

	}




	// this function will set the paths of the vertices and extrusion files.
	// 	we will also open the files in read mode in this function.
	void set_parse_files( char * vpath, char * epath ){
		path_file_vertex = vpath;
		path_file_extrusions = epath;

		file_vertex = fopen( path_file_vertex, "r" );
		file_extrusions = fopen( path_file_extrusions, "r" );

		if( file_vertex != NULL && file_extrusions != NULL ){

			file_rendering = true;
			std::string temp = path_file_vertex;

			std::cout << temp.substr( temp.size()-4, temp.size() ) << "\n";
			path_file_vertex_normalized = (char * )"";


		}else{
			std::cout << "ERROR: unable to open file for reading. OBJECT\n";
		}



	}


	

	void normalize_data(){

		if( file_rendering == false ){

			float max = 0;
			
			float min_x = 0;
			float max_x = 0;

			float min_y = 0;
			float max_y = 0;

			float min_z = 0;
			float max_z = 0;

			for( int i = 0; i < data_len; i++ ){
				
				// this is to calculate the max possible and normalize.
				if( data[i].x_start > max ){
					max = data[i].x_start;
				}

				if( data[i].y_start > max ){
					max = data[i].y_start;
				}

				if( data[i].z_start > max ){
					max = data[i].z_start;
				}

				if( data[i].x_end > max ){
					max = data[i].x_end;
				}

				if( data[i].y_end > max ){
					max = data[i].y_end;
				}

				if( data[i].z_end > max ){
					max = data[i].z_end;
				}	
			}

			max = max * 1.3;

			for( int i = 0; i < data_len; i++ ){
				
				data[i].x_start = data[i].x_start / max;
				data[i].y_start = data[i].y_start / max;
				data[i].z_start = data[i].z_start / max;

				data[i].x_end = data[i].x_end / max;
				data[i].y_end = data[i].y_end / max;
				data[i].z_end = data[i].z_end / max;
			
			}

			MAX_X = max_x;
			MAX_Y = max_y;
			MAX_Z = max_z;

		}


	}


	// get the average x value.
	float get_center_x(){
		float min = 9999;
		float max = -9999;
		float start;
		float end;

		for( int i = 0; i < data_len; i++ ){
			
			start = data[i].x_start * (*scaling);
			end = data[i].x_end * (*scaling);

			if( start < min ){
				min = start;
			}

			if( start > max ){
				max = start;
			}

			if( end < min ){
				min = end;
			}

			if( end > max ){
				max = end;
			}			
		}
	
		MAX_X = max;
	
		return (min + max) / 2;

	}


	// get the average y value
	float get_center_y(){
		float min = 9999;
		float max = -9999;
		float start;
		float end;

		for( int i = 0; i < data_len; i++ ){
			
			start = data[i].y_start * (*scaling);
			end = data[i].y_end * (*scaling);

			if( start < min ){
				min = start;
			}

			if( start > max ){
				max = start;
			}

			if( end < min ){
				min = end;
			}

			if( end > max ){
				max = end;
			}			
		}

		MAX_Y = max;

		return (min + max) / 2;
	}


	// get the average y value
	float get_center_z(){
		float min = 9999;
		float max = -9999;
		float start;
		float end;

		for( int i = 0; i < data_len; i++ ){

			start = data[i].z_start * (*scaling);
			end = data[i].z_end * (*scaling);
			
			if( start < min ){
				min = start;
			}

			if( start > max ){
				max = start;
			}

			if( end < min ){
				min = end;
			}

			if( end > max ){
				max = end;
			}			
		}

		MAX_Z = max;

		return (min + max) / 2;
	}


	void set_rotation_vars( float * _x, float * _y, float * _z ){
		angle_phi = _x;
		angle_theta = _y;
		angle_z = _z;
	}


	void set_scaling_var( float * s ){
		scaling = s;
	}

	void set_position_vars( float * _x, float * _y, float * _z ){
		look_at_x = _x;
		look_at_y = _y;
		look_at_z = _z;
	}

	void set_gcode_range_vars( int * _start, int * _end ){
		render_start = _start;
		render_end = _end;
	}


	void normalize_thickness(){
		float max = 0;
		for( int i = 0; i < data_len; i++ ){
		
			if( data[i].thickness > max && data[i].thickness < MAX_EXTRUDE_DIFF )
				max = data[i].thickness;	
		}

		for( int i = 0; i < data_len; i++ ){
			data[i].thickness = (data[i].thickness / max) * MAX_DRAW_LINE_THICKNESS;

		}
	}



	////////////////// RENDER //////////////////


	void render(){
		for( int i = (*render_start); i < data_len && i < (*render_end); i++ ){
			data[i].render( *scaling, MAX_Z );
		}
	}



	void render_from_file(){
		
	}



	////////////////////////////////////////////



	void mouse_press( int _button, int _state, int _x, int _y ){
		
		// we will now check for a rotation
		// 	check for the middle mouse, button == 1
		//	check whether it is a press down or up
		if( _button == 1 && _state == 0 ){ 			// press down.	
			current_x = _x;
			current_y = _y;
			rotating = true;

		}else if( _button == 1 && _state == 1 ){

			current_x = _x;
			current_y = _y;
			rotating = false;
		
		}else if( _button == 3 ){		// scroll in
			*scaling += SCROLL_SCALE_RATE;
			// recenter to the object
			*look_at_x = get_center_x();
			*look_at_y = get_center_y();
			*look_at_z = get_center_z();

		}else if( _button == 4 ){		// scroll out
			*scaling -= SCROLL_SCALE_RATE;

			*look_at_x = get_center_x();
			*look_at_y = get_center_y();
			*look_at_z = get_center_z();
		
		}else if( _button == 2 && _state == 0){		// left mouse click DOWN
			current_x = _x;
			current_y = _y;
			panning = true;
		
		}else if( _button == 2 && _state == 1 ){
			current_x = _x;
			current_y = _y;
			panning = false;	
		}

	}

	// triggers when the mouse is pressed and the mouse is moving
	void mouse_move_active( int _x, int _y ){
		
		if( rotating == true){
			*angle_phi 	 	+=	- (_y -  current_y) * ROTATE_RATE;

			if( *angle_phi > 180 ){
				*angle_phi = 180;
			
			}else if( *angle_phi < 1 ){
				*angle_phi = 1;
			}

			*angle_theta 	+=	(_x -  current_x) * ROTATE_RATE;
			
			current_x = _x;
			current_y = _y;
		}

		if( panning == true ){					// MUST FIX IN FU
			*look_at_x += (_x - current_x) * PAN_RATE_X;
			*look_at_y += (_y - current_y) * PAN_RATE_Y;
		}

	}


};


//****************************************************************************************************************//
//****************************************************************************************************************//
//****************************************************************************************************************//

class GUI_SLIDER{
public:
	int max_val;
	int min_val;
	int * control_val;

	float x;
	float y;
	float height;
	float width;
	float select_x;
	float select_y;
	float select_width;
	float select_height;

	GUI_SLIDER(){
		min_val = 0;
		max_val = 100;
		x = 0;
		y = 0;
		width = 0.2;
		height = 0.2;
		control_val = new int[0];
		*control_val = 0;
	}

	void set_range( int s, int e ){
		min_val = 0;
		max_val = 200;
	}

	void set_control_val( int * _control ){
		control_val = _control;
	}

	void set_control_val( int a ){
		* control_val = a;
	}

	void increment_slider(){
		*control_val += (max_val - min_val) / 10;

		if( *control_val > max_val ){
			*control_val = max_val;
		}
	}

	void decrement_slider(){
		*control_val += (max_val - min_val) / 10;	
	
		if( *control_val < min_val ){
			*control_val = min_val;
		}
	}





	void set_size( float _w, float _h ){
		width = _w;
		height = _h;
	}

	void set_position( float _x, float _y ){
		x = _x;
		y = _y;
	}

	void render(){

		// first we will draw the bar in the background.
		glColor3f( 0.5, 0.5, 0.5 );
		glBegin( GL_QUADS );
			glVertex3f( x + 2* width/6, 	y, 			0.0 );
			glVertex3f( x + 3* width/6, 	y, 			0.0 );
			glVertex3f( x + 3* width/6, 	y + height, 0.0 );
			glVertex3f( x + 2* width/6, 	y + height, 0.0 );
		glEnd();

		// now we will draw the main bar that is to be scrolled
		glColor3f( 0, 0, 0 );
		float draw_pos = (*control_val) /(max_val-min_val) - height/2;
		glBegin( GL_QUADS );
			glVertex3f( x, 			draw_pos, 				0.0 );
			glVertex3f( x + width, 	draw_pos, 				0.0 );
			glVertex3f( x + width, 	draw_pos + height/40, 	0.0 );
			glVertex3f( x, 			draw_pos + height/40, 	0.0 );
		glEnd();


	}



};



class GUI_GCODE_PARSER{
public:

	GUI_SLIDER * sliders;
	int sliders_len;



	GUI_GCODE_PARSER(){
		std::cout << "created a new GUI" << "\n";
		sliders = new GUI_SLIDER[0];
		sliders_len = 0;	
	}

	void add_slider( GUI_SLIDER s ){
		GUI_SLIDER * temp = new GUI_SLIDER[ sliders_len + 1 ];
		std::copy( sliders, sliders + sliders_len, temp );
		delete[] sliders;
		sliders = temp;
		sliders[ sliders_len ] = s;
		sliders_len += 1;
	}


	void render(){
		// first render the sliders
		for( int i = 0; i < sliders_len; i++ ){
			sliders[i].render();
		}

		
	}


};







//****************************************************************************************************************//
//****************************************************************************************************************//
//****************************************************************************************************************//




class GCODE_MANAGER{
public:
	FILE * file;
	char * path;

	FILE * file_vertex;
	char * path_file_vertex;

	FILE * file_extrusions;
	char * path_file_extrusions;

	bool render_from_file;		// if True, would render from a file.

	GCODE_OBJECT object;
	GUI_GCODE_PARSER gui;

	int test_i;
	long int gcode_len;

	GCODE_MANAGER( char * p ){
		path = p;
		file = fopen( p , "r" );
		object = GCODE_OBJECT();
		gui = GUI_GCODE_PARSER();
		test_i = 0;
		render_from_file = false;
		gcode_len = 0;
	}



	////////////////////////////////////////////////////////////// PARSING //////////////////////////////////////////////////////////////////////////////////////



	void parse(){

		char * line = ( char * ) malloc( sizeof( char ) * MAX_LINE_INPUT );
		std::string * line_arr;
		int line_arr_len = 0;

		bool got_home = false;
		int data_len_needed = 0;

		float head_x = 0;
		float head_y = 0;
		float head_z = 0;
		float extrude_amount = 0;
		float head_speed;

		// the variable to store new info for every line
		// index:	0	1	2	3	4
		// value:	x 	y 	z 	e 	f
		float * line_val_store = new float[ 5 ];




		float x_current = 0;
		float y_current = 0;
		float z_current = 0;
		float e_current = 0;
		float f_current = 0;

		float x_new = 0;
		float y_new = 0;
		float z_new = 0;
		float e_new = 0;
		float f_new = 0;


		while( fgets( line, MAX_LINE_INPUT, file ) ){
			/////////
			// this s the loop that reads the lines in the file.
			// our goal is:
			//	1: igonre all lines where the first charecter is a semicolon, as that is a comment.
			//	2: Parse the gcode in layers. Collect all the layers in their own data types. They will have their own class.
			//	3: ...
			/////////

			// split the array using the ' ' as a delimiter. The first element is the length of the array
			line_arr = split(line, ' ');
			line_arr_len = string_to_int( line_arr[0] );
			// check the length of the array and whether the line is a comment ( starts with ';')
			if( line_arr_len != 0 && line_arr[1][0] != ';'  ){
				// we know the line is not a comment. Now we have to check if it part of the commands we understand. These commands will
				// be listed as defines in the top.

				// first we check the linear move command.
				// This command has multiple inputs. We will read these inputs and start building our layer. We must keep track of the head.

				// move command
				if( line_arr[1] == COMMAND_LINEARMOVE ){
					
					// parse the line and store the commands
					line_val_store = parse_move_command( line_arr );
					
					// store the new values.
					x_new = line_val_store[ 0 ];
					y_new = line_val_store[ 1 ];
					z_new = line_val_store[ 2 ];
					e_new = line_val_store[ 3 ];
					f_new = line_val_store[ 4 ];

					// these statements keep the motors in place, if they didnt turn. This will cause an error when going to home. 
					/// POTENTIAL ERROR IF INTENDED POSITION IS 0,0,0
					if( x_new == 0 ){
						x_new = x_current;
					}

					if( y_new == 0 ){
						y_new = y_current;
					}

					if( z_new == 0 ){
						z_new = z_current;
					}

					// create the object.
					if(test_i > 12 && e_new != 0 && e_new - e_current > MIN_EXTRUDE_DIFF ){
						data_len_needed += 1;
					}test_i += 1;


					x_current = x_new;
					y_current = y_new;
					z_current = z_new;
					e_current = e_new;

				}

			}	
		}










		object.reset_data_size( data_len_needed );
		gcode_len = data_len_needed;


		x_current = 0;
		y_current = 0;
		z_current = 0;
		e_current = 0;
		f_current = 0;

		x_new = 0;
		y_new = 0;
		z_new = 0;
		e_new = 0;
		f_new = 0;

		test_i = 0;

		int current_i = 0;
		fseek(file, 0, SEEK_SET);

		while( fgets( line, MAX_LINE_INPUT, file ) ){
			/////////
			// this s the loop that reads the lines in the file.
			// our goal is:
			//	1: igonre all lines where the first charecter is a semicolon, as that is a comment.
			//	2: Parse the gcode in layers. Collect all the layers in their own data types. They will have their own class.
			//	3: ...
			/////////

			// split the array using the ' ' as a delimiter. The first element is the length of the array
			line_arr = split(line, ' ');
			line_arr_len = string_to_int( line_arr[0] );
			// check the length of the array and whether the line is a comment ( starts with ';')
			if( line_arr_len != 0 && line_arr[1][0] != ';'  ){
				// we know the line is not a comment. Now we have to check if it part of the commands we understand. These commands will
				// be listed as defines in the top.

				// first we check the linear move command.
				// This command has multiple inputs. We will read these inputs and start building our layer. We must keep track of the head.

				// move command
				if( line_arr[1] == COMMAND_LINEARMOVE ){
					
					// parse the line and store the commands
					line_val_store = parse_move_command( line_arr );
					
					// store the new values.
					x_new = line_val_store[ 0 ];
					y_new = line_val_store[ 1 ];
					z_new = line_val_store[ 2 ];
					e_new = line_val_store[ 3 ];
					f_new = line_val_store[ 4 ];

					// these statements keep the motors in place, if they didnt turn. This will cause an error when going to home. 
					/// POTENTIAL ERROR IF INTENDED POSITION IS 0,0,0
					if( x_new == 0 ){
						x_new = x_current;
					}

					if( y_new == 0 ){
						y_new = y_current;
					}

					if( z_new == 0 ){
						z_new = z_current;
					}

					// create the object.
					if(test_i > 12 && e_new != 0 && e_new - e_current > MIN_EXTRUDE_DIFF ){

						object.set_data( current_i, x_current, y_current, z_current, x_new, y_new, z_new, 0.2 );
						current_i += 1;
					}test_i += 1;


					x_current = x_new;
					y_current = y_new;
					z_current = z_new;
					e_current = e_new;

				}

			}	
		}


		object.normalize_data();
		//object.normalize_thickness();

	}





	////////////////////////////////////////////////////////////// PARSING FILE //////////////////////////////////////////////////////////////////////////////////////



	// this is the function that will add the data into a file for the gcode object to read in the future.
	// 	WE are using it because we are fingding that storing all the gcode commands as extrusion objects requires too much memory. 
	//	This will parse all these commands into 2 differenet files. One with pure vertex data. Another with extrusion data. 
	//	If we use this method. we can store much more information within extrusions and can draw the object using the vertex data file.
	//		VERTEX DATA FORMAT: 
	//			x y R G B.
	void parse_into_file( char * _path_vertex, char * _path_extrusion ){
		path_file_vertex = _path_vertex;
		path_file_extrusions = _path_extrusion;

		file_extrusions = fopen( path_file_extrusions, "w+" );
		file_vertex = fopen( path_file_vertex, "w+" );

		render_from_file = true; // this will direct the render function to render from a file.

		// we know for sure not that we hav access to these files in write mode. We will parse and input the data into the file.
		if( file_vertex != NULL && file_extrusions != NULL ){

			// we will get the data for every extrusion. Exactyl the same way we did up top.

			char * line = ( char * ) malloc( sizeof( char ) * MAX_LINE_INPUT );
			std::string * line_arr;
			int line_arr_len = 0;

			bool got_home = false;

			float head_x = 0;
			float head_y = 0;
			float head_z = 0;
			float extrude_amount = 0;
			float head_speed;

			// the variable to store new info for every line
			// index:	0	1	2	3	4
			// value:	x 	y 	z 	e 	f
			std::string * line_val_store = new std::string[ 5 ];

			std::string x_current = "";
			std::string y_current = "";
			std::string z_current = "";
			std::string e_current = "";
			std::string f_current = "";

			std::string x_new = "";
			std::string y_new = "";
			std::string z_new = "";
			std::string e_new = "";
			std::string f_new = "";

			std::string file_vertex_send;
			std::string file_extrusions_send;


			while( fgets( line, MAX_LINE_INPUT, file ) ){
				/////////
				// this s the loop that reads the lines in the file.
				// our goal is:
				//	1: igonre all lines where the first charecter is a semicolon, as that is a comment.
				//	2: Parse the gcode in layers. Collect all the layers in their own data types. They will have their own class.
				//	3: ...
				/////////

				// split the array using the ' ' as a delimiter. The first element is the length of the array
				line_arr = split(line, ' ');
				line_arr_len = string_to_int( line_arr[0] );
				// check the length of the array and whether the line is a comment ( starts with ';')
				if( line_arr_len != 0 && line_arr[1][0] != ';'  ){
					// we know the line is not a comment. Now we have to check if it part of the commands we understand. These commands will
					// be listed as defines in the top.

					// first we check the linear move command.
					// This command has multiple inputs. We will read these inputs and start building our layer. We must keep track of the head.

					// move command
					if( line_arr[1] == COMMAND_LINEARMOVE ){
						
						// parse the line and store the commands
						line_val_store = parse_move_command_giveString( line_arr );
						
						// store the new values.
						x_new = line_val_store[ 0 ];
						y_new = line_val_store[ 1 ];
						z_new = line_val_store[ 2 ];
						e_new = line_val_store[ 3 ];
						f_new = line_val_store[ 4 ];

						// these statements keep the motors in place, if they didnt turn. This will cause an error when going to home. 
						/// POTENTIAL ERROR IF INTENDED POSITION IS 0,0,0
						if( x_new == "" ){
							x_new = x_current;
						}

						if( y_new == "" ){
							y_new = y_current;
						}

						if( z_new == "" ){
							z_new = z_current;
						}

						if( e_new == "" ){
							e_new = e_current;
						}

						if( f_new == "" ){
							f_new = f_current;
						}





						file_vertex_send =  x_current;
						file_vertex_send += " ";
						file_vertex_send += y_current;
						file_vertex_send += " ";
						file_vertex_send += z_current;

						file_extrusions_send = file_vertex_send;
						file_extrusions_send += " ";

						file_extrusions_send += e_current;

						file_extrusions_send += " ";
						file_extrusions_send += "\n";



						file_vertex_send += "\n";

								


						// create the object.
						if( test_i > 12 && e_new != "" ){
						
							fputs(file_vertex_send.c_str(), file_vertex); 
							fputs(file_extrusions_send.c_str(), file_extrusions); 
						
						}test_i += 1;

						x_current = x_new;
						y_current = y_new;
						z_current = z_new;
						e_current = e_new;
						f_current = f_new;

					}

				}	
			}

			fclose (file_vertex);
			fclose (file_extrusions);


			object.set_parse_files( path_file_vertex, path_file_extrusions );


		}else{
			std::cout << "ERROR, could not open files\n";
		}

	}



	////////////////////////////////////////////////////////////// END //////////////////////////////////////////////////////////////////////////////////////


	void render(){
		// if we want to render the verts strored directly in stack
		if( render_from_file == false ){
			object.render();
		
		}else{
			object.render_from_file();
		}
	}

	void render_gui(){
		gui.render();
	}

	void mouse_press( int _button, int _state, int _x, int _y ){
		object.mouse_press( _button, _state, _x, _y );
	}

	void mouse_move_active( int _x, int _y ){
		object.mouse_move_active( _x, _y );
	}

	void set_rotation_vars(float * _x, float * _y, float * _z ){
		object.set_rotation_vars( _x, _y, _z );
	}

	float get_center_x(){
		return object.get_center_x();
	}

	float get_center_y(){
		return object.get_center_y();	
	}

	float get_center_z(){
		return object.get_center_z();	
	}

	void set_scaling_var( float * s ){
		object.set_scaling_var( s );
	}

	void set_gcode_range_vars( int * _s, int * _e ){
		object.set_gcode_range_vars( _s, _e );
	}

	void set_position_vars( float * _x, float * _y, float * _z ){
		object.set_position_vars( _x, _y, _z );
	}

	long int get_gcode_len(){
		return gcode_len;
	}

	//////////////////////// GUI funcs ////////////////////////////////////

	void add_to_gui( GUI_SLIDER s ){
		gui.add_slider( s );
	}


};


//****************************************************************************************************************//
//****************************************************************************************************************//
//****************************************************************************************************************//

GCODE_MANAGER parser = GCODE_MANAGER( ( char * ) "etc/trump.gcode" );


//****************************************************************************************************************//
//****************************************************************************************************************//
//****************************************************************************************************************//

float x_multiple = 0.0;
float y_multiple = 0.0;
float z_multiple = 0.0;


void render( int a ){
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

		x_multiple = (20*sin(GLOBAL_ROTATE_X * PI / 180)*cos(GLOBAL_ROTATE_Y * PI / 180));
		y_multiple = (20*sin(GLOBAL_ROTATE_X * PI / 180)*sin(GLOBAL_ROTATE_Y * PI / 180));
		z_multiple = (20*cos(GLOBAL_ROTATE_X * PI / 180));

		gluLookAt( 	OBJECT_MIDDLE_X, 					OBJECT_MIDDLE_Y, 					OBJECT_MIDDLE_Z, 
					10*x_multiple + OBJECT_MIDDLE_X, 	10*y_multiple + OBJECT_MIDDLE_Y, 	10*z_multiple + OBJECT_MIDDLE_Z,
					0.0, 								0.0, 								1.0 	);
	
		parser.render();

	glPopMatrix();

	parser.render_gui();


	glutSwapBuffers();
	glutTimerFunc( FRAME_TIMEOUT, render, 0 );


}




//****************************************************************************************************************//
void callback_display( void ){
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



	glBegin( GL_QUADS );
		glVertex3f( 0.0, 0.0, 0.0 );
		glVertex3f( 0.1, 0.0, 0.0 );
		glVertex3f( 0.1, 0.1, 0.0 );
		glVertex3f( 0.0, 0.1, 0.0 );
	glEnd();

	glutSwapBuffers();

	glutTimerFunc( 10, render, 0 );
}


void callback_active_mousemove( int _x, int _y ){
	parser.mouse_move_active( _x, _y );
}

void callback_mouse_press( int _button, int _state, int _x, int _y ){
	parser.mouse_press( _button, _state, _x, _y );	
}


void callback_key_press( unsigned char _key, int _x, int _y ){
	std::cout << _key << " " << _x << " " << _y << "\n";
}

//****************************************************************************************************************//

std::string int_to_string( int num ){
	std::ostringstream str;
	str << num;
	return str.str();
}


int string_to_int( std::string str ){
	return atoi( str.c_str() );
}

float string_to_float( std::string str ){
	return atof( str.c_str() );
}


std::string * split( char * input, char delim ){

	int i = 0;
	std::string part = ""; 

	std::string * out = new std::string[1];
	int out_len = 1;
	out[0] = int_to_string(out_len);

	std::string * temp = new std::string[out_len];

	while( input[i] != '\0' ){

		if( input[i] == delim ){
			// we will add the current part to the string array then empty it.
			// adding the part to the array
			temp = new std::string[ out_len + 1 ];
			std::copy( out, out + out_len, temp );
			out = temp;
			out[ out_len ] = part;
			out_len += 1;
			// clearing the part
			part = "";
			i += 1;
			// recalc the length
			out[0] = int_to_string(out_len);
		}
		part.push_back( input[i] );
		i += 1;
	}

	// add the last element
	temp = new std::string[ out_len + 1 ];
	std::copy( out, out + out_len, temp );
	out = temp;
	out[ out_len ] = part;
	out_len += 1;
	// recalc the length
	out[0] = int_to_string(out_len);

	return out;

}



float * parse_move_command( std::string * input ){
	// create an output with the size of 5 for:
	//		x y z extrude feedrate
	float * output = new float[ 5 ];
	int input_len = string_to_int(input[0]);

						// defaults.
	output[ 0 ] = 0;	// x = 0
	output[ 1 ] = 0;	// y = 0
	output[ 2 ] = 0;	// z = 0
	output[ 3 ] = 0;	// E = 0
	output[ 4 ] = 0;	// F = 0

	for( int i = 2; i < input_len; i++ ){
		
		if( input[i][0] == 'X' || input[i][0] == 'x' ){
			output[0] = string_to_float(input[i].substr( 1, input[i].size() ));

		}else if( input[i][0] == 'Y' || input[i][0] == 'y' ){
			output[1] = string_to_float(input[i].substr( 1, input[i].size() ));
		
		}else if( input[i][0] == 'Z' || input[i][0] == 'z' ){
			output[2] = string_to_float(input[i].substr( 1, input[i].size() ));

		}else if( input[i][0] == 'E' || input[i][0] == 'e' ){
			output[3] = string_to_float(input[i].substr( 1, input[i].size() ));

		}else if( input[i][0] == 'F' || input[i][0] == 'f' ){
			output[4] = string_to_float(input[i].substr( 1, input[i].size() ));

		}

	}

	return output;
}






// MUST FIX !!!!! 


std::string * parse_move_command_giveString(std::string * input){
	// create an output with the size of 5 for:
	//		x y z extrude feedrate
	std::string * output = new std::string[ 5 ];
	int input_len = string_to_int(input[0]);

						// defaults.
	output[ 0 ] = "";	// x = 0
	output[ 1 ] = "";	// y = 0
	output[ 2 ] = "";	// z = 0
	output[ 3 ] = "";	// E = 0
	output[ 4 ] = "";	// F = 0

	std::string temp;

	for( int i = 2; i < input_len; i++ ){
		
		temp = input[i].substr( 1, input[i].size() );

		if( input[i].substr( input[i].size() - 1, input[i].size() ) == "\n" ){ 
			temp = input[i].substr( 1, input[i].size() - 2 );
		}



		if( input[i][0] == 'X' || input[i][0] == 'x' ){
			if( string_to_float( temp ) != 0 ){
				output[0] = temp;
			}

		}else if( input[i][0] == 'Y' || input[i][0] == 'y' ){
			if( string_to_float( temp ) != 0 ){
				output[1] = temp;
			}

		}else if( input[i][0] == 'Z' || input[i][0] == 'z' ){
			if( string_to_float( temp ) != 0 ){	
				output[2] = temp;
			}

		}else if( input[i][0] == 'E' || input[i][0] == 'e' ){
			if( string_to_float( temp ) != 0 ){
					output[3] = temp;
			}

		}else if( input[i][0] == 'F' || input[i][0] == 'f' ){
			if( string_to_float( temp ) != 0 ){	
				output[4] = temp;
			}

		}

	}

	return output;


}


//****************************************************************************************************************//

int main( int argc, char * * argv ){

	std::cout << "starting gcode parser" << "\n";

	parser.set_rotation_vars( &GLOBAL_ROTATE_X, &GLOBAL_ROTATE_Y, &GLOBAL_ROTATE_Z );
	parser.set_scaling_var( &OBJECT_SCALING );
	parser.set_position_vars( &OBJECT_MIDDLE_X, &OBJECT_MIDDLE_Y, &OBJECT_MIDDLE_Z );
	parser.set_gcode_range_vars( &GLOBAL_GCODE_START, &GLOBAL_GCODE_END );

	GLOBAL_GCODE_START = 0;
	GLOBAL_GCODE_END = 999999;

	// this method does not allow me to read big files
	parser.parse();

	// gui

	GUI_SLIDER gcode_slider = GUI_SLIDER();
	gcode_slider.set_size( 0.1, 1.6 );
	gcode_slider.set_position( 0.8, -0.8 );
	gcode_slider.set_range( 0, parser.get_gcode_len() );
	gcode_slider.set_control_val( &GLOBAL_GCODE_END );

	parser.add_to_gui( gcode_slider );

	// this method will store the data into 2 different files. One with lots of info and the other with little info to draw from.
	//parser.parse_into_file( (char *)"vertex_data.txt", (char *)"extrusion_data.txt" );

	OBJECT_SCALING = 1;

	OBJECT_MIDDLE_X = parser.get_center_x();
	OBJECT_MIDDLE_Y = parser.get_center_y();
	OBJECT_MIDDLE_Z = parser.get_center_z();



	// setting the screen size
	WINDOW_WIDTH = 1200;
	WINDOW_HEIGHT = 900;
	GLOBAL_ROTATE_X = 1;
	GLOBAL_ROTATE_Y = 1;
	GLOBAL_ROTATE_Z = 0.0;
	GLOBAL_TRANSFORM_X = 0.0;
	GLOBAL_TRANSFORM_Y = 0.0;
	GLOBAL_TRANSFORM_Z = 0.0;


	// intializing glut
	glutInit( &argc, argv );
	glutInitWindowPosition( 0, 0 );
	glutInitWindowSize( WINDOW_WIDTH, WINDOW_HEIGHT );
	glutCreateWindow( "gcode parser" );
	glutInitDisplayMode( GL_RGBA | GL_DOUBLE | GL_DEPTH );

	glColor3f( 0.0, 0.0, 0.0 );
	glClearColor( 1.0, 1.0, 1.0, 0.0 );

	//gluPerspective( 90, (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT, 0.01, 1);
	//glFrustum( -1, 1, -1, 1, 0.01, 1 );

	glutDisplayFunc( callback_display );
	glutMotionFunc( callback_active_mousemove );
	glutMouseFunc( callback_mouse_press ); 
	glutKeyboardFunc( callback_key_press ); 


	glutMainLoop();


	return 0;
}

//****************************************************************************************************************//