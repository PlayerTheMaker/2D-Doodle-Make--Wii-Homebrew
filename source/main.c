/*---------------------------------------------------------------------------------

	Simple demonstration of sprites using textured quads

---------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/tpl.h>

#include "textures_tpl.h"
#include "textures.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

static void *frameBuffer[2] = { NULL, NULL};
static GXRModeObj *rmode;

#define NUM_SPRITES 100

//cursor struct
typedef struct {
	float x,y;			// screen coordinates
	bool connected;
	bool free;
	u32 pressedInputs, heldInputs, releasedInputs;
	int lastX,lastY;
	int item;
}Cursor;

typedef struct {
	float x,y,vx,vy,rot,rotv,size;
	Cursor* parent;
	bool active, grounded;
}Player;

typedef struct {
	float x,y,x2,y2;
}Line;

GXTexObj texObj;

#define TEXTURE_SIZE 128
void drawSquareSprite(float tx, float ty, float tscale, float x, float y, float scale, float rot, bool flip);
void drawLine(float x, float y, float x2, float y2, float width, bool grey);
bool lineCircleOverlap(float x, float y, float x2, float y2, float cx, float cy, float r);
bool pointCircleOverlap(float x, float y, float cx, float cy, float r);
bool pointLineOverlap(float x, float y, float x2, float y2, float px, float py);
float distance(float x, float y, float x2, float y2);
int sideOfLine(float x, float y, float x2,float y2, float px, float py);

//---------------------------------------------------------------------------------
int main( int argc, char **argv ){
//---------------------------------------------------------------------------------
	u32	fb; 	// initial framebuffer index
	u32 first_frame;
	f32 yscale;
	u32 xfbHeight;
	Mtx44 perspective;
	Mtx GXmodelView2D;
	void *gp_fifo = NULL;

	GXColor background = {0, 0, 0, 0xff};

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	fb = 0;
	first_frame = 1;
	// allocate 2 framebuffers for float buffering
	frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(frameBuffer[fb]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	fb ^= 1;

	// setup the fifo and then init the flipper
	gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gp_fifo,0,DEFAULT_FIFO_SIZE);

	GX_Init(gp_fifo,DEFAULT_FIFO_SIZE);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);


	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer[fb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);


	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);


	GX_InvalidateTexAll();

	TPLFile spriteTPL;
	TPL_OpenTPLFromMemory(&spriteTPL, (void *)textures_tpl,textures_tpl_size);
	TPL_GetTexture(&spriteTPL,ballsprites,&texObj);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	guOrtho(perspective,0,479,0,639,0,300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	// Initialise the console, required for printf
	CON_Init(frameBuffer[0], 20, 20, rmode->fbWidth-20, rmode->xfbHeight-20, rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	
	//controller
	WPAD_Init();
	//tell wpad to output the IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);

	//random num generator init
	srand(time(NULL));
	
	//add cursors and players
	Cursor cursors[4];
	Player players[4];
	Cursor defaultCursor = {.x=1000,.y=100,.free=true,.connected=false,.item=0,.lastX=0,.lastY=0};
	Player defaultPlayer = {.x=200,.y=200, .vx = 0, .vy = 0, .rot = 0, .rotv = 0, .size = 1.25, .active = false, .grounded = false};
	for (int i=0; i<=3; i++){
		cursors[i] = defaultCursor;
		players[i] = defaultPlayer;
		players[i].parent = &cursors[i];
	}

	//world vars
	float gravity = 0.1;
	float movePower = 0.3;
	float turnPower = 0.1;
	float jumpPower = 1;
	float airMovePower = 0.1;

	float airFriction = .99;
	float groundFriction = .965;
	float surfaceAdherence = 0.5;
	
	bool screenWrap = false;
	bool bottomAbyss = false;
	
	//object pools
	int bgLineCount = 0;
	Line bgLines[2500];
	int lineCount = 0;
	Line lines[2500];
	//lineCount++;
	//Line simpleLine = {.x = 10, .y = 10, .x2 = 100, .y2 = 100};
	//lines[0] = simpleLine;

	int frameLooper = -1;

	while(1) {
		
		frameLooper++;
		if(frameLooper >= 30){
			frameLooper = 0;
		}

		//GAME CALCULATIONS
		WPAD_ScanPads();

	
		//exit if home button is pressed on P1 controller
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);
		
		
		
		//get input data
		for(int i=0; i<=3; i++){
			WPADData* data = WPAD_Data(i);
			if(data->data_present){
				cursors[i].x = (int)(data->ir.x*1.135);
				cursors[i].y = (int)(data->ir.y*1.135);
				cursors[i].connected = true;
				cursors[i].pressedInputs = WPAD_ButtonsDown(i);
				cursors[i].heldInputs = WPAD_ButtonsHeld(i);
				cursors[i].releasedInputs = WPAD_ButtonsUp(i);
			}else{
				cursors[i] = defaultCursor;
			}
		}
		
		//act on cursor inputs
		for(int i=0; i<=3; i++){
			//cursors
			if(cursors[i].connected){
				Cursor* currCursor = &cursors[i];
				
				//enable player
				if(currCursor->pressedInputs & WPAD_BUTTON_PLUS){
					players[i].active = true;
					players[i].x = currCursor->x;
					players[i].y = currCursor->y;
				}

				//cursor add elements
				if(currCursor->pressedInputs & WPAD_BUTTON_A){
					if(currCursor->item <= 1){//drawing
						cursors[i].lastX = currCursor->x;
						cursors[i].lastY = currCursor->y;
					}else{
						
					}
				}

				//erase
				if(currCursor->heldInputs & WPAD_BUTTON_B){
					for(int j = 0; j < lineCount; j++){
						Line* currLine = &lines[j];
						if(lineCircleOverlap(currLine->x,currLine->y,currLine->x2,currLine->y2,currCursor->x,currCursor->y,2)  ){
							for(int k = j+1; k < lineCount; k++){
								lines[k-1] = lines[k];
							}
							lineCount--;
						}
					}
				}
				
				//line drag
				if(currCursor->item <= 1 && currCursor->heldInputs & WPAD_BUTTON_A){
					//add line
					if( !pointCircleOverlap(currCursor->x,currCursor->y,currCursor->lastX,currCursor->lastY,10)){

						int x = currCursor->x;
						int y = currCursor->y;
						int x2 = currCursor->lastX;
						int y2 = currCursor->lastY;

						//make sure lines are always go from left to right
						if(x < x2){
							int store = x;
							x = x2;
							x2 = store;
							store = y;
							y = y2;
							y2 = store;
						}

						if(currCursor->item == 0 && lineCount < 2500){
							lines[lineCount] = (Line){.x=x,.y=y,.x2=x2,.y2=y2};
							lineCount++;
						}
						if(currCursor->item == 1 && bgLineCount < 2500){
							bgLines[bgLineCount] = (Line){.x=x,.y=y,.x2=x2,.y2=y2};
							bgLineCount++;
						}
						cursors[i].lastX = currCursor->x;
						cursors[i].lastY = currCursor->y;
					}
				}
				
			}

			//players
			if(players[i].active){
				if(cursors[i].connected){

					Player* currPlayer = &players[i];

					//grav
					currPlayer->vy += gravity;

					//move
					if(currPlayer->grounded){
						if(cursors[i].heldInputs & WPAD_BUTTON_RIGHT){
							currPlayer->vx += cosf(currPlayer->rot) * movePower;
							currPlayer->vy += sinf(currPlayer->rot) * movePower;
						}
						if(cursors[i].heldInputs & WPAD_BUTTON_LEFT){
							currPlayer->vx -= cosf(currPlayer->rot) * movePower;
							currPlayer->vy -= sinf(currPlayer->rot) * movePower;
						}
						//jump
						if(cursors[i].heldInputs & WPAD_BUTTON_UP){
							currPlayer->vx += cosf(currPlayer->rot - M_PI/2) * jumpPower;
							currPlayer->vy += sinf(currPlayer->rot - M_PI/2) * jumpPower;
						}
					//rotate
					}else{
						if(cursors[i].heldInputs & WPAD_BUTTON_RIGHT){
							currPlayer->rot += turnPower;
							currPlayer->vx += airMovePower;
						}
						if(cursors[i].heldInputs & WPAD_BUTTON_LEFT){
							currPlayer->rot -= turnPower;
							currPlayer->vx -= airMovePower;
						}
					}

					while(currPlayer->rot > M_PI){
						currPlayer->rot -= 2*M_PI;
					}
					while(currPlayer->rot < -M_PI){
						currPlayer->rot += 2*M_PI;
					}
					

					//apply velocity for target pos
					int targetX = currPlayer->x + currPlayer->vx;
					int targetY = currPlayer->y + currPlayer->vy;
					
					currPlayer->grounded = false;
					for(int j = 0; j < lineCount; j++){
						
						//general area check
						if(
							lineCircleOverlap(
							lines[j].x, lines[j].y, lines[j].x2, lines[j].y2,
							targetX, targetY, currPlayer->size * 8 * 2)
						){
							float groundAngle = 
								atan2f( (lines[j].y-lines[j].y2), (lines[j].x-lines[j].x2) );

							if(sideOfLine(lines[j].x,lines[j].y,lines[j].x2,lines[j].y2,currPlayer->x,currPlayer->y) > 0){
								groundAngle = atan2f( (lines[j].y2-lines[j].y), (lines[j].x2-lines[j].x) );
							}


							if( abs(currPlayer->rot - groundAngle) < M_PI/3){
								currPlayer->grounded = true;
								currPlayer->rot = groundAngle;
							}

							float groundPerpendicular = groundAngle - M_PI/2;

							//collision loop
							int loopCount = 0;
							while(
								lineCircleOverlap(
							lines[j].x, lines[j].y, lines[j].x2, lines[j].y2,
							targetX, targetY, currPlayer->size * 8)
							){
								targetX = currPlayer->x - currPlayer->vx;
								targetY = currPlayer->y - currPlayer->vy;
								currPlayer->vx += cosf(groundPerpendicular) * .1;
								currPlayer->vy += sinf(groundPerpendicular) * .1;
								targetX = currPlayer->x + currPlayer->vx;
								targetY = currPlayer->y + currPlayer->vy;
								
								loopCount += 1;
								if(loopCount > 100){
									targetX = currPlayer->x;
									targetY = currPlayer->y;
									currPlayer->vx = 0;
									currPlayer->vy = 0;
									break;
								}
							}

							if(cursors[i].heldInputs & WPAD_BUTTON_UP == false){
								currPlayer->vx -= cosf(groundPerpendicular)*surfaceAdherence;
								currPlayer->vy -= sinf(groundPerpendicular)*surfaceAdherence;
							}
							
						}
					}

					currPlayer->x = targetX;
					currPlayer->y = targetY;

					//friction
					currPlayer->vx *= airFriction;
					currPlayer->vy *= airFriction;
					if(currPlayer->grounded){
						currPlayer->vx *= groundFriction;
						currPlayer->vy *= groundFriction;
					}
					

				}else{
					players[i] = defaultPlayer;
				}
			}
		}
		
		
		//DRAW!
		
		//def not where I'm supposed to put print statements lol
		/*
		printf("\33[2K\r");
		if(data->data_present){
			printf("wiimote %d: x -> %f y-> %f angle -> %f\n", 0, data->ir.x, data->ir.y, data->ir.angle);
		}
		*/
		//printf("\33[2K\r");
		//printf("%d, %f",lineCount,lines[0].x2);
		
		
		GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
		GX_InvVtxCache();
		GX_InvalidateTexAll();

		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

		guMtxIdentity(GXmodelView2D);
		guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -5.0F);
		GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);
		
		//background giant sprite
		drawSquareSprite(0,1,1, 320,240,100,0, false);
		
		//time to actually draw the game!
		
		//bg lines
		for(int i = 0; i < bgLineCount; i++){
			Line* line = &bgLines[i];
			drawLine(line->x,line->y,line->x2,line->y2,3,true);
		}
		//lines
		for(int i = 0; i < lineCount; i++){
			Line* line = &lines[i];
			drawLine(line->x,line->y,line->x2,line->y2,3,false);
		}
		
		
		for(int i=0; i<=3; i++){
			//draw cursors and half-drawn lines
			Cursor* currCursor = &cursors[i];
			if (currCursor->connected){
				if((currCursor->item <= 1) && (currCursor->heldInputs & WPAD_BUTTON_A)){
					drawLine(currCursor->x,currCursor->y,currCursor->lastX,currCursor->lastY,3, currCursor->item == 1);
				}
				drawSquareSprite(i+0.025f,0.025f,0.95f, currCursor->x,currCursor->y+10,1.5f,0, false);

				//erase icon
				if((currCursor->heldInputs & WPAD_BUTTON_B) || (currCursor->heldInputs & WPAD_BUTTON_A && currCursor->item == 2)){
					drawSquareSprite(0.05,2.05,0.9, currCursor->x+10,currCursor->y,1,0, false);
				}

			}

			//draw players
			Player* currPlayer = &players[i];
			if (currPlayer->active){
				int frameOff = 0;

				if((currCursor->heldInputs & WPAD_BUTTON_RIGHT) || (currCursor->heldInputs & WPAD_BUTTON_LEFT)){
					if(frameLooper > 7 && frameLooper < 15 || frameLooper > 22){
						frameOff = 2;
					}
				}

				drawSquareSprite(4.1+frameOff, 0.1+i*2, 1.875, currPlayer->x,currPlayer->y,currPlayer->size,currPlayer->rot, false);
			}
		}
		
		//the value bar (cause I can't get printf to work without inducing seziures)
		drawLine(0, 0, frameLooper*3, 0, 10, true);
			
		GX_DrawDone();
		
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fb],GX_TRUE);


		VIDEO_SetNextFramebuffer(frameBuffer[fb]);
		if(first_frame) {
			VIDEO_SetBlack(false);
			first_frame = 0;
		}
		VIDEO_Flush();
		VIDEO_WaitVSync();
		fb ^= 1;		// flip framebuffer
	}
	return 0;
}

//---------------------------------------------------------------------------------
void drawSquareSprite(float tx, float ty, float tscale, float x, float y, float scale, float rot, bool flip) {
//---------------------------------------------------------------------------------
	
	float flipVal = 1;
	if(flip){
		flipVal = -1;
	}

	float tleft = (tx*16)/TEXTURE_SIZE;
	float ttop = (ty*16)/TEXTURE_SIZE;
	float tlength = (tscale*16)/TEXTURE_SIZE;
	float drawscale = tscale*16*scale*0.5;
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x-cosf(rot+M_PI/4)*drawscale*flipVal, y-sinf(rot+M_PI/4)*drawscale);			// Top Left
		GX_TexCoord2f32(tleft,ttop);
		GX_Position2f32(x-cosf(rot+3*(M_PI/4))*drawscale*flipVal, y-sinf(rot+3*(M_PI/4))*drawscale);	// Top Right
		GX_TexCoord2f32(tleft+tlength,ttop);
		GX_Position2f32(x-cosf(rot+5*(M_PI/4))*drawscale*flipVal, y-sinf(rot+5*(M_PI/4))*drawscale);	// Bottom Right
		GX_TexCoord2f32(tleft+tlength,ttop+tlength);
		GX_Position2f32(x-cosf(rot-M_PI/4)*drawscale*flipVal, y-sinf(rot-M_PI/4)*drawscale);			// Bottom Left
		GX_TexCoord2f32(tleft,ttop+tlength);
	GX_End();									// Done Drawing The Quad

}


void drawLine(float x, float y, float x2, float y2, float width, bool grey){
	
	float perpendicular = atan2f(y2-y,x2-x)-0.5f*(float)M_PI;
	
	float xOff = cosf(perpendicular)*width*.5;
	float yOff = sinf(perpendicular)*width*.5;
	
	float greyOff = 0;
	if(grey) greyOff = 1;
	
	//printf("\33[2K\r");
	//printf("%f",xOff);
	
	float tleft = (1.1f*16+greyOff*16)/TEXTURE_SIZE;
	float ttop = (1.1f*16)/TEXTURE_SIZE;
	float tlength = (.1f*16)/TEXTURE_SIZE;
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad
		GX_Position2f32(x+xOff, y+yOff);					// Top Left
		GX_TexCoord2f32(tleft,ttop);
		GX_Position2f32(x2+xOff, y2+yOff);			// Top Right
		GX_TexCoord2f32(tleft+tlength,ttop);
		GX_Position2f32(x2-xOff,y2-yOff);	// Bottom Right
		GX_TexCoord2f32(tleft+tlength,ttop+tlength);
		GX_Position2f32(x-xOff,y-yOff);			// Bottom Left
		GX_TexCoord2f32(tleft,ttop+tlength);
	GX_End();									// Done Drawing The Quad
}

bool lineCircleOverlap(float x, float y, float x2, float y2, float cx, float cy, float r){
	
	if(
		pointCircleOverlap(x, y, cx, cy, r) ||
		pointCircleOverlap(x2, y2, cx, cy, r)
	){
		return true;
	}

	float len = distance(x,y,x2,y2);

	//I don't get this dot product stuff
	//just took it from https://www.jeffreythompson.org/collision-detection/line-circle.php lol
	//like, how do multiplied vectors help us find the closest point? and why divide by len^2?
	float dot = ( ((cx-x)*(x2-x)) + ((cy-y)*(y2-y)) ) / pow(len,2);

	float closestX = x + (dot * (x2-x));
	float closestY = y + (dot * (y2-y));

	bool onSegment = pointLineOverlap(x,y,x2,y2, closestX,closestY);
	if (!onSegment) return false;

	float dist = distance(cx,cy,closestX,closestY);
	if(dist <= r){
		return true;
	}
	return false;
}

bool pointCircleOverlap(float x, float y, float cx, float cy, float r){
	if(distance(x,y,cx,cy) <= r){
		return true;
	}
	return false;
}

bool pointLineOverlap(float x, float y, float x2, float y2, float px, float py) {

  //distance
  float distance1 = distance(px,py, x,y);
  float distance2 = distance(px,py, x,y);

  float lineLen = distance(x,y, x2,y2);

  float buffer = 15; 
  //if distances to points roughly equal line length
  if (distance1+distance2 >= lineLen-buffer && distance1+distance2 <= lineLen+buffer) {
    return true;
  }
  return false;
}

float distance(float x, float y, float x2, float y2){
	return sqrtf( (x-x2)*(x-x2) + (y-y2)*(y-y2));
}

int sideOfLine(float x, float y, float x2,float y2, float px, float py) {
  if( (x2 - x)*(py - y) - (y2 - y)*(px - x) > 0){
	return -1;
  }else{
	return 1;
  }
}