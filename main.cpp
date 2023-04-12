#include "engine/engine.h"
#include "engine/geometry.h"
#include "engine/3d.h"
#include "engine/shaders.h"
#include "engine/renderer2d.h"
#include "engine/strings.h"

#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "time.h"
#include <cstring>
#include <vector>

enum EntityType{
	ENTITY_TYPE_PLAYER,
	ENTITY_TYPE_ENEMY,
	ENTITY_TYPE_BULLET,
};

struct Body{
	Vec2f pos;
	Vec2f size;
};

struct Physics{
	Vec2f velocity;
	Vec2f acceleration;
	Vec2f resistance;
	bool onGround;
};

struct EnemyAI{
	bool shouldJump;
	int shouldShoot;
	int clock;
};

struct Entity{
	Body body;
	Body lastBody;
	Physics physics;
	EnemyAI enemyAI;
	enum EntityType type;
};

struct Particle{
	size_t ID;
	Vec2f pos;
	Vec2f lastPos;
	Vec2f velocity;
	Vec2f acceleration;
	Vec2f resistance;
};

struct Pixel{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;

	bool operator==(Pixel compPixel){
		return r == compPixel.r && g == compPixel.g && b == compPixel.b && a == compPixel.a;
	}

	bool operator!=(Pixel compPixel){
		return r != compPixel.r || g != compPixel.g || b != compPixel.b || a != compPixel.a;
	}
};

Pixel BACKGROUND_COLOR = { 0, 0, 0, 255 };
Pixel ROCK_COLOR = { 255, 255, 255, 255 };
Pixel STATIC_ROCK_COLOR = { 100, 100, 100, 255 };
Pixel WATER_COLOR = { 100, 100, 255, 255 };

Vec4f PLAYER_COLOR = { 0.0, 0.0, 1.0, 1.0 };
Vec4f ENEMY_COLOR = { 1.0, 0.0, 0.0, 1.0 };
Vec4f BULLET_COLOR = { 1.0, 1.0, 0.0, 1.0 };

float PARTICLE_GRAVITY = 0.1;
float PARTICLE_COLLISION_DAMPENING = 0.7;

float PLAYER_GRAVITY = 0.15;
float PLAYER_JUMP_SPEED = 3.2;
float PLAYER_WALK_SPEED = 0.17;
float PLAYER_WALK_RESISTANCE = 0.93;
float PLAYER_JUMP_RESISTANCE = 1.0;
float PLAYER_STOP_JUMP_RESISTANCE = 0.5;

float ENEMY_WALK_SPEED = 0.20;
float ENEMY_WALK_RESISTANCE = 0.93;
float ENEMY_DETECTION_RADIUS = 200.0;
float ENEMY_JUMP_SPEED = 4.5;

float BULLET_DESTROY_RADIUS = 10;
float BULLET_SPEED = 4;

int WIDTH = 480;
int HEIGHT = 270;

Renderer2D_Renderer renderer;

std::vector<Entity> entities;

int GRID_WIDTH = 480 * 4;
int GRID_HEIGHT = 270 * 4;

std::vector<Particle> particles;

Pixel *drawingGrid;
Pixel *staticParticlesGrid;
int *collisionIndexGrid;
int *emptyCollisionIndexGrid;
Texture gridTexture;

Vec2f bendingPos;
bool isBending = false;
float BENDING_FORCE = 0.01;
float BENDING_RADIUS = 20;
float BIG_BENDING_RADIUS = 80;

float CAMERA_SPEED = 20;
Vec2f cameraPos;
Vec2f cameraDest;

size_t currentParticleID = 0;

bool firstFrame = true;

int getGridIndex(Vec2f pos){
	return GRID_WIDTH * (int)pos.y + (int)pos.x;
}

Vec2f getGridPos(int index){
	return getVec2f((int)(index / GRID_WIDTH), index % GRID_WIDTH);
}

bool checkOub(Vec2f pos){
	return pos.x < 0 || pos.y < 0 || pos.x >= GRID_WIDTH || pos.y >= GRID_HEIGHT;
}

void Body_init(Body *body_p, Vec2f pos, Vec2f size){
	body_p->pos = pos;
	body_p->size = size;
}

bool checkBodyVec2fCol(Body body, Vec2f v){
	return v.x >= body.pos.x
		&& v.x < body.pos.x + body.size.x
		&& v.y >= body.pos.y
		&& v.y < body.pos.y + body.size.y;
}

void Physics_init(Physics *physics_p){
	physics_p->velocity = getVec2f(0.0, 0.0);
	physics_p->acceleration = getVec2f(0.0, 0.0);
	physics_p->resistance = getVec2f(1.0, 1.0);
}

void Particle_init(Particle *particle_p, Vec2f pos){

	particle_p->ID = currentParticleID;
	currentParticleID++;

	particle_p->pos = pos;
	particle_p->velocity = getVec2f(0.0, 0.0);
	particle_p->acceleration = getVec2f(0.0, 0.0);
	particle_p->resistance = getVec2f(0.97, 0.97);
}

void Entity_init(Entity *entity_p, Vec2f pos, Vec2f size, enum EntityType type){

	Body_init(&entity_p->body, pos, size);
	Physics_init(&entity_p->physics);

	entity_p->type = type;

}

void addPlayer(Vec2f pos){

	Entity entity;

	Entity_init(&entity, pos, getVec2f(15.0, 20.0), ENTITY_TYPE_PLAYER);
	entity.physics.resistance.x = PLAYER_WALK_RESISTANCE;

	entities.push_back(entity);

}

void addEnemy(Vec2f pos){

	Entity entity;

	Entity_init(&entity, pos, getVec2f(15.0, 20.0), ENTITY_TYPE_ENEMY);

	entity.physics.resistance.x = ENEMY_WALK_RESISTANCE;

	entities.push_back(entity);

}

void addBullet(Vec2f pos, Vec2f velocity){

	Entity entity;

	Entity_init(&entity, pos, getVec2f(10.0, 10.0), ENTITY_TYPE_BULLET);

	entity.physics.velocity = velocity;

	entities.push_back(entity);


}

void paintArea(int inputX, int inputY, int w, int h, Pixel color){
	for(int x = 0; x < w; x++){
		for(int y = 0; y < h; y++){

			Vec2f pos = getVec2f(inputX + x, inputY + y);
			int index = getGridIndex(pos);

			if(!checkOub(pos)){
				staticParticlesGrid[index] = color;
			}

		}
	}
}

void Engine_start(){

	printf("Starting the engine\n");

	Engine_setWindowSize(WIDTH * 2, HEIGHT * 2);

	Engine_centerWindow();

	Renderer2D_init(&renderer, WIDTH, HEIGHT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//init world
	addPlayer(getVec2f(100.0, GRID_HEIGHT - 200.0));

	//addEnemy(getVec2f(400.0, 100.0));

	staticParticlesGrid = (Pixel *)malloc(sizeof(Pixel) * GRID_WIDTH * GRID_HEIGHT);
	drawingGrid = (Pixel *)malloc(sizeof(Pixel) * GRID_WIDTH * GRID_HEIGHT);
	collisionIndexGrid = (int *)malloc(sizeof(int) * GRID_WIDTH * GRID_HEIGHT);
	emptyCollisionIndexGrid = (int *)malloc(sizeof(int) * GRID_WIDTH * GRID_HEIGHT);

	for(int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++){
		staticParticlesGrid[i] = BACKGROUND_COLOR;
		emptyCollisionIndexGrid[i] = -1;
	}

	//create world geometry
	{

		paintArea(0, GRID_HEIGHT - 100, WIDTH, 100, ROCK_COLOR);

		/*
		int posX = 0;

		posX += 300;

		paintArea(posX, GRID_HEIGHT - 125, 100, 25, ROCK_COLOR);

		posX += 330;

		paintArea(posX, GRID_HEIGHT - 200, 100, 100, ROCK_COLOR);

		posX += 300;

		paintArea(posX, GRID_HEIGHT - 200, 100, 100, ROCK_COLOR);

		posX += 200;

		paintArea(0, HEIGHT - 100, posX, 100, ROCK_COLOR);

		posX += 200;

		paintArea(posX, HEIGHT - 100, 300, 100, ROCK_COLOR);

		posX += 300;

		paintArea(posX, HEIGHT - 200, 100, 200, STATIC_ROCK_COLOR);

		posX += 100;

		paintArea(posX, HEIGHT - 100, 500, 100, STATIC_ROCK_COLOR);
		*/
	
	}

	cameraPos = getVec2f(0.0, 0.0);
	cameraDest = getVec2f(0.0, 0.0);

}

void Engine_update(float deltaTime){

	printf("---\n");

	if(Engine_keys[ENGINE_KEY_Q].down){
		Engine_quit();
	}

	//control bending
	bendingPos = getVec2f(Engine_pointer.pos.x / ((float)Engine_clientWidth / (float)WIDTH) - cameraPos.x, Engine_pointer.pos.y / ((float)Engine_clientHeight / (float)HEIGHT) - cameraPos.y);

	if(Engine_pointer.down){
		isBending = true;
	}else{
		isBending = false;
	}

	if(Engine_pointer.downed){

		bool notOnlyRocks = false;

		for(int x = 0; x < BENDING_RADIUS * 2; x++){
			for(int y = 0; y < BENDING_RADIUS * 2; y++){

				Vec2f pos = getVec2f(bendingPos.x - BENDING_RADIUS + x, bendingPos.y - BENDING_RADIUS + y);
				int index = getGridIndex(pos);

				if(getMagVec2f(getVec2f(x - BENDING_RADIUS, y - BENDING_RADIUS)) <= BENDING_RADIUS
				&& !checkOub(pos)
				&& staticParticlesGrid[index] == BACKGROUND_COLOR){
					notOnlyRocks = true;
				}
			
			}
		}

		for(int x = 0; x < BENDING_RADIUS * 2; x++){
			for(int y = 0; y < BENDING_RADIUS * 2; y++){

				Vec2f pos = getVec2f(bendingPos.x - BENDING_RADIUS + x, bendingPos.y - BENDING_RADIUS + y);
				int index = getGridIndex(pos);

				if(getMagVec2f(getVec2f(x - BENDING_RADIUS, y - BENDING_RADIUS)) <= BENDING_RADIUS
				&& !checkOub(pos)
				&& staticParticlesGrid[index] == ROCK_COLOR
				&& notOnlyRocks){

					staticParticlesGrid[index] = BACKGROUND_COLOR;

					Particle particle;
					Particle_init(&particle, pos);

					particles.push_back(particle);

				}
			
			}
		}
	}

	//handle entity physics
	for(int i = 0; i < entities.size(); i++){

		Entity *entity_p = &entities[i];

		entity_p->physics.acceleration = getVec2f(0.0, 0.0);

		//control players
		if(entity_p->type == ENTITY_TYPE_PLAYER){

			if(Engine_keys[ENGINE_KEY_A].down){
				entity_p->physics.acceleration.x += -PLAYER_WALK_SPEED;
			}
			if(Engine_keys[ENGINE_KEY_D].down){
				entity_p->physics.acceleration.x += PLAYER_WALK_SPEED;
			}
			if(Engine_keys[ENGINE_KEY_W].down
			&& entity_p->physics.onGround){
				entity_p->physics.acceleration.y += -PLAYER_JUMP_SPEED;
			}

			entity_p->physics.resistance.y = PLAYER_JUMP_RESISTANCE;

			if(!Engine_keys[ENGINE_KEY_W].down
			&& entity_p->physics.velocity.y < 0){
				entity_p->physics.resistance.y = PLAYER_STOP_JUMP_RESISTANCE;
			}

		}

		//control enemies
		if(entity_p->type == ENTITY_TYPE_ENEMY){

			entity_p->enemyAI.clock++;

			Vec2f playerPos;
			for(int j = 0; j < entities.size(); j++){
				if(entities[j].type == ENTITY_TYPE_PLAYER){
					playerPos = getAddVec2f(entities[j].body.pos, getDivVec2fFloat(entities[j].body.size, 2.0));
				}
			}

			Vec2f enemyPos = getAddVec2f(entity_p->body.pos, getDivVec2fFloat(entity_p->body.size, 2.0));

			bool aggro = false;

			entity_p->enemyAI.shouldJump = false;

			if(getMagVec2f(getSubVec2f(playerPos, enemyPos)) <= ENEMY_DETECTION_RADIUS){

				aggro = true;

				entity_p->enemyAI.shouldJump = true;
				
			}

			if(aggro){

				float direction = 1.0;
				if(entity_p->body.pos.x + entity_p->body.size.x > playerPos.x){
					direction = -1.0;
				}

				entity_p->physics.acceleration.x += ENEMY_WALK_SPEED * direction;

				if(entity_p->enemyAI.shouldJump
				&& entity_p->physics.onGround){
					entity_p->physics.acceleration.y -= ENEMY_JUMP_SPEED;
				}

				bool shouldShoot = false;
				if(entity_p->enemyAI.clock % 30 == 0){
					shouldShoot = true;
				}

				if(shouldShoot){

					Vec2f velocity = getSubVec2f(playerPos, enemyPos);
					Vec2f_normalize(&velocity);
					Vec2f_mulByFloat(&velocity, BULLET_SPEED);
					
					addBullet(enemyPos, velocity);

				}
			
			}

		}

		if(entity_p->type == ENTITY_TYPE_PLAYER
		|| entity_p->type == ENTITY_TYPE_ENEMY){
			entity_p->physics.acceleration.y += PLAYER_GRAVITY;
		}

		Vec2f_add(&entity_p->physics.velocity, entity_p->physics.acceleration);
		Vec2f_mul(&entity_p->physics.velocity, entity_p->physics.resistance);

		entity_p->physics.onGround = false;

		entity_p->lastBody = entity_p->body;
	
	}

	//handle particle physics
	for(int i = 0; i < particles.size(); i++){

		Particle *particle_p = &particles[i];

		particle_p->acceleration = getVec2f(0.0, 0.0);

		particle_p->acceleration.y += PARTICLE_GRAVITY;

		//bending force
		if(isBending){
			Vec2f force = getSubVec2f(bendingPos, particle_p->pos);
			Vec2f_mulByFloat(&force, BENDING_FORCE);
			Vec2f_add(&particle_p->acceleration, force);
		}

		Vec2f_add(&particle_p->velocity, particle_p->acceleration);

		Vec2f_mul(&particle_p->velocity, particle_p->resistance);

		particle_p->lastPos = particle_p->pos;

	}

	//move and collide things
	for(int c = 0; c < 2; c++){

		//move entities
		for(int i = 0; i < entities.size(); i++){

			Entity *entity_p = &entities[i];

			entity_p->body.pos[c] += entity_p->physics.velocity[c];
		
		}

		std::vector<int> removedParticleIDs;

		//move particles
		for(int i = 0; i < particles.size(); i++){

			Particle *particle_p = &particles[i];

			particle_p->pos[c] += particle_p->velocity[c];

		}

		//handle static particle collisions
		for(int i = 0; i < particles.size(); i++){

			Particle *particle_p = &particles[i];

			if(checkOub(particle_p->pos)){
				continue;
			}

			int index = getGridIndex(particle_p->pos);

			if(staticParticlesGrid[index] == ROCK_COLOR){

				Vec2f pos = particle_p->pos;
				bool foundEmptySpot = false;
				int steps = 0;

				while(!checkOub(pos)){

					steps++;

					{
						Vec2f newPos = pos;
						newPos[c] += steps;

						int newIndex = getGridIndex(newPos);

						if(!checkOub(newPos)
						&& staticParticlesGrid[newIndex] == BACKGROUND_COLOR){
							foundEmptySpot = true;
							pos = newPos;
							break;
						}
					}
					{
						Vec2f newPos = pos;
						newPos[c] -= steps;

						int newIndex = getGridIndex(newPos);

						if(!checkOub(newPos)
						&& staticParticlesGrid[newIndex] == BACKGROUND_COLOR){
							foundEmptySpot = true;
							pos = newPos;
							break;
						}
					}

				}

				if(foundEmptySpot){

					bool particleIsBended = false;

					if(isBending
					&& getMagVec2f(getSubVec2f(particle_p->pos, bendingPos)) <= BENDING_RADIUS){
						particleIsBended = true;
					}

					if(particleIsBended){
						particle_p->pos = pos;
						particle_p->velocity[c] = 0.0;
					}else{
						int newIndex = getGridIndex(pos);

						staticParticlesGrid[newIndex] = ROCK_COLOR;

						particles.erase(particles.begin() + i, particles.begin() + i + 1);
					}

					i = -1;
					continue;

				}else{

					particles.erase(particles.begin() + i, particles.begin() + i + 1);
					i--;
					continue;
				}
			}
			
		}

		//put particles into collision index grid
		memcpy(collisionIndexGrid, emptyCollisionIndexGrid, sizeof(int) * GRID_WIDTH * GRID_HEIGHT);

		for(int i = 0; i < particles.size(); i++){

			if(checkOub(particles[i].pos)){
				continue;
			}

			int index = getGridIndex(particles[i].pos);
			
			collisionIndexGrid[index] = i;
		
		}

		//handle moving particle and static rock collisions
		for(int i = 0; i < particles.size(); i++){

			Particle *particle_p = &particles[i];

			if(checkOub(particle_p->pos)){
				continue;
			}

			int index = getGridIndex(particle_p->pos);

			if((collisionIndexGrid[index] != -1
			&& collisionIndexGrid[index] != i)
			|| staticParticlesGrid[index] == STATIC_ROCK_COLOR){

				Vec2f pos = particle_p->pos;
				bool foundEmptyCell = false;
				int steps = 0;

				while(!checkOub(pos)){
					
					steps++;

					{
						Vec2f newPos = pos;
						newPos[c] += steps;

						int newIndex = getGridIndex(newPos);

						if(collisionIndexGrid[newIndex] == -1
						&& collisionIndexGrid[newIndex] != i
						&& staticParticlesGrid[newIndex] == BACKGROUND_COLOR){
							pos = newPos;
							foundEmptyCell = true;
							break;
						}
					
					}
					{
						Vec2f newPos = pos;
						newPos[c] -= steps;

						int newIndex = getGridIndex(newPos);

						if(collisionIndexGrid[newIndex] == -1
						&& collisionIndexGrid[newIndex] != i
						&& staticParticlesGrid[newIndex] == BACKGROUND_COLOR){
							pos = newPos;
							foundEmptyCell = true;
							break;
						}
					
					}

				}

				if(foundEmptyCell){

					particle_p->pos = pos;

					particle_p->velocity[c] *= PARTICLE_COLLISION_DAMPENING;

					int newIndex = getGridIndex(pos);

					collisionIndexGrid[newIndex] = i;

				}else{
					removedParticleIDs.push_back(particle_p->ID);
				}

			}

		}

		//handle particles oub
		for(int i = 0; i < particles.size(); i++){

			Particle *particle_p = &particles[i];

			if(checkOub(particle_p->pos)){

				Vec2f pos = particle_p->pos;
				bool foundEmptyCell = false;
				int steps = 0;

				for(int j = 0; j < GRID_WIDTH; j++){
					
					steps++;

					{
						Vec2f newPos = pos;
						newPos[c] += steps;

						int newIndex = getGridIndex(newPos);

						if(!checkOub(newPos)
						&& collisionIndexGrid[newIndex] == -1
						&& collisionIndexGrid[newIndex] != i
						&& staticParticlesGrid[newIndex] == BACKGROUND_COLOR){
							pos = newPos;
							foundEmptyCell = true;
							break;
						}
					
					}
					{
						Vec2f newPos = pos;
						newPos[c] -= steps;

						int newIndex = getGridIndex(newPos);

						if(!checkOub(newPos)
						&& collisionIndexGrid[newIndex] == -1
						&& collisionIndexGrid[newIndex] != i
						&& staticParticlesGrid[newIndex] == BACKGROUND_COLOR){
							pos = newPos;
							foundEmptyCell = true;
							break;
						}
					
					}

				}

				if(foundEmptyCell){

					particle_p->pos = pos;

					particle_p->velocity[c] *= PARTICLE_COLLISION_DAMPENING;

					int newIndex = getGridIndex(pos);

					collisionIndexGrid[newIndex] = i;

				}else{
					removedParticleIDs.push_back(particle_p->ID);
				}

			}

		}

		//handle entity particle collisions for players and enemies
		for(int i = 0; i < entities.size(); i++){

			Entity *entity_p = &entities[i];

			if(!(entity_p->type == ENTITY_TYPE_PLAYER
			|| entity_p->type == ENTITY_TYPE_ENEMY)){
				continue;
			}

			//handle player moving particle collisions
			for(int x = 0; x < entity_p->body.size.x; x++){
				for(int y = 0; y < entity_p->body.size.y; y++){

					Vec2f pos = entity_p->body.pos;

					pos.x += x;
					pos.y += y;

					if(checkOub(pos)){
						continue;
					}

					int index = getGridIndex(pos);

					if(collisionIndexGrid[index] != -1){

						Particle *particle_p = &particles[collisionIndexGrid[index]];

						float entityCenter = entity_p->lastBody.pos[c] + entity_p->lastBody.size[c] / 2.0;
						float particlePos = particle_p->lastPos[c];

						if(particlePos < entityCenter){
							entity_p->body.pos[c] = (int)pos[c] + 1;
						}else{

							entity_p->body.pos[c] = (int)(pos[c] - entity_p->body.size[c]);

							if(c == 1){
								entity_p->physics.onGround = true;
							}

						}
						
						entity_p->physics.velocity[c] = 0.0;

					}
				
				}
			}

			//handle entities static particle collisions
			for(int x = 0; x < entity_p->body.size.x; x++){
				for(int y = 0; y < entity_p->body.size.y; y++){

					Vec2f pos = entity_p->body.pos;
					pos.x += x;
					pos.y += y;

					if(checkOub(pos)){
						continue;
					}

					int index = getGridIndex(pos);

					if(staticParticlesGrid[index] == ROCK_COLOR
					|| staticParticlesGrid[index] == STATIC_ROCK_COLOR){

						Particle *particle_p = &particles[collisionIndexGrid[index]];

						float entityCenter = entity_p->lastBody.pos[c] + entity_p->lastBody.size[c] / 2.0;
						float particlePos = pos[c];

						if(particlePos < entityCenter){
							entity_p->body.pos[c] = (int)pos[c] + 1;
						}else{

							entity_p->body.pos[c] = (int)(pos[c] - entity_p->body.size[c]);

							if(c == 1){
								entity_p->physics.onGround = true;
							}

						}
						
						entity_p->physics.velocity[c] = 0.0;

					}
				
				}
			}

			//handle entity oub
			if(entity_p->body.pos[c] < 0.0){
				entity_p->body.pos[c] = 0.0;
			}
			if(entity_p->body.pos[c] + entity_p->body.size[c] > GRID_WIDTH){
				entity_p->body.pos[c] = GRID_WIDTH - entity_p->body.size[c];
			}

			//handle player moving particle collisions second time
			for(int x = 0; x < entity_p->body.size.x; x++){
				for(int y = 0; y < entity_p->body.size.y; y++){

					Vec2f pos = entity_p->body.pos;

					pos.x += x;
					pos.y += y;

					if(checkOub(pos)){
						continue;
					}

					int index = getGridIndex(pos);

					if(collisionIndexGrid[index] != -1){

						Particle *particle_p = &particles[collisionIndexGrid[index]];

						Vec2f checkPos = pos;
						int steps = 0;

						while(true){

							steps++;
							{
								checkPos = pos;
								checkPos[c] += steps;
								int checkIndex = getGridIndex(checkPos);

								if(!checkOub(checkPos)
								&& collisionIndexGrid[checkIndex] == -1
								&& staticParticlesGrid[checkIndex] == BACKGROUND_COLOR
								&& (checkPos[c] < entity_p->body.pos[c] || checkPos[c] > entity_p->body.pos[c] + entity_p->body.size[c])){
									
									particle_p->pos = checkPos;
									collisionIndexGrid[checkIndex] = collisionIndexGrid[index];
									break;

								}
							
							}
							{
								checkPos = pos;
								checkPos[c] -= steps;
								int checkIndex = getGridIndex(checkPos);

								if(!checkOub(checkPos)
								&& collisionIndexGrid[checkIndex] == -1
								&& staticParticlesGrid[checkIndex] == BACKGROUND_COLOR
								&& (checkPos[c] < entity_p->body.pos[c] || checkPos[c] > entity_p->body.pos[c] + entity_p->body.size[c])){

									particle_p->pos = checkPos;
									collisionIndexGrid[checkIndex] = collisionIndexGrid[index];
									break;

								}
							
							}
						
						}

						collisionIndexGrid[index] = -1;

					}
				
				}

			}
		
		}

		//handle entity particle collisions for bullets
		for(int i = 0; i < entities.size(); i++){

			Entity *entity_p = &entities[i];

			if(!(entity_p->type == ENTITY_TYPE_BULLET)){
				continue;
			}

			bool hit = false;

			for(int x = 0; x < entity_p->body.size.x; x++){
				for(int y = 0; y < entity_p->body.size.y; y++){

					Vec2f pos = entity_p->body.pos;

					pos.x += x;
					pos.y += y;

					if(checkOub(pos)){
						continue;
					}

					int index = getGridIndex(pos);

					if(collisionIndexGrid[index] != -1
					|| staticParticlesGrid[index] != BACKGROUND_COLOR){
						hit = true;
					}
				}
			}

			if(hit){
				for(int x = 0; x < BULLET_DESTROY_RADIUS * 2; x++){
					for(int y = 0; y < BULLET_DESTROY_RADIUS * 2; y++){

						Vec2f pos = getVec2f(entity_p->body.pos.x + entity_p->body.size.y / 2.0 - BULLET_DESTROY_RADIUS + x, entity_p->body.pos.y + entity_p->body.size.y / 2.0 - BULLET_DESTROY_RADIUS + y);
						int index = getGridIndex(pos);

						if(getMagVec2f(getVec2f(x - BULLET_DESTROY_RADIUS, y - BULLET_DESTROY_RADIUS)) <= BULLET_DESTROY_RADIUS
						&& !checkOub(pos)){

							if(collisionIndexGrid[index] != -1){
								removedParticleIDs.push_back(particles[collisionIndexGrid[index]].ID);
							}

							if(staticParticlesGrid[index] == ROCK_COLOR){
								staticParticlesGrid[index] = BACKGROUND_COLOR;
							}
						
						}

					}
				}

				entities.erase(entities.begin() + i, entities.begin() + i + 1);
				i--;
				continue;
			}

		}

		//remove particles
		for(int i = 0; i < particles.size(); i++){
			for(int j = 0; j < removedParticleIDs.size(); j++){
				
				if(particles[i].ID == removedParticleIDs[j]){
					particles.erase(particles.begin() + i, particles.begin() + i + 1);
					i--;
					break;
				}

			}
		}

	}

	//handle camera
	{

		Entity *player_p = &entities[0];

		float playerPointerDiffX = bendingPos.x - player_p->body.pos.x;
		float playerPointerDiffY = bendingPos.y - player_p->body.pos.y;

		playerPointerDiffX *= 0.4;
		playerPointerDiffY *= 0.2;

		cameraDest.x = (WIDTH / 2.0 - (player_p->body.pos.x + player_p->body.size.x / 2.0)) - playerPointerDiffX;
		cameraDest.y = (HEIGHT / 2.0 - (player_p->body.pos.y + player_p->body.size.y / 2.0)) - playerPointerDiffY;

		if(cameraDest.x > 0){
			cameraDest.x = 0;
		}
		if(cameraDest.y > 0){
			cameraDest.y = 0;
		}
		if(cameraDest.x < -(GRID_WIDTH - WIDTH)){
			cameraDest.x = -(GRID_WIDTH - WIDTH);
		}
		if(cameraDest.y < -(GRID_HEIGHT - HEIGHT)){
			cameraDest.y = -(GRID_HEIGHT - HEIGHT);
		}

		Vec2f diff = getSubVec2f(cameraDest, cameraPos);
		Vec2f_divByFloat(&diff, CAMERA_SPEED);

		Vec2f_add(&cameraPos, diff);

		if(firstFrame){
			cameraPos = cameraDest;
		}

		renderer.offset = cameraPos;
	
	}

	firstFrame = false;

}

void Engine_draw(){

	Renderer2D_updateDrawSize(&renderer, Engine_clientWidth, Engine_clientHeight);

	Renderer2D_clear(&renderer);

	//draw grid
	memcpy(drawingGrid, staticParticlesGrid, sizeof(Pixel) * GRID_WIDTH * GRID_HEIGHT);

	//place rock particles in grid
	for(int i = 0; i < particles.size(); i++){

		Particle *particle_p = &particles[i];

		if(checkOub(particle_p->pos)){
			continue;
		}
		
		int index = getGridIndex(particle_p->pos);

		drawingGrid[index] = ROCK_COLOR;

	}

	Renderer2D_setShader(&renderer, renderer.textureShader);

	Texture_init(&gridTexture, "grid", (unsigned char *)drawingGrid, GRID_WIDTH, GRID_HEIGHT);

	Renderer2D_setTexture(&renderer, gridTexture);

	Renderer2D_drawRectangle(&renderer, 0, 0, GRID_WIDTH, GRID_HEIGHT);

	Texture_free(&gridTexture);

	Renderer2D_setShader(&renderer, renderer.colorShader);

	Renderer2D_setRotation(&renderer, 0.0);

	//draw entities
	for(int i = 0; i < entities.size(); i++){

		Entity *entity_p = &entities[i];

		if(entity_p->type == ENTITY_TYPE_PLAYER){
			Renderer2D_setColor(&renderer, PLAYER_COLOR);
		}
		if(entity_p->type == ENTITY_TYPE_ENEMY){
			Renderer2D_setColor(&renderer, ENEMY_COLOR);
		}
		if(entity_p->type == ENTITY_TYPE_BULLET){
			Renderer2D_setColor(&renderer, BULLET_COLOR);
		}

		Renderer2D_drawRectangle(&renderer, (int)entity_p->body.pos.x, (int)entity_p->body.pos.y, entity_p->body.size.x, entity_p->body.size.y);
	
	}

	//bending pos
	//Renderer2D_setColor(&renderer, getVec4f(1.0, 0.0, 0.0, 1.0));

	//Renderer2D_drawRectangle(&renderer, bendingPos.x, bendingPos.y, 10, 10);


}

void Engine_finnish(){

}
