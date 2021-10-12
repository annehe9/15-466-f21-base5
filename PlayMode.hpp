#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <array>
#include <cmath>
#include <math.h>

#define PI_F 3.14159265f
#define DEG2RAD 3.14159265f / 180.f
#define RADIUS 12
#define MAX_DEPTH 500.f

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	uint32_t score = 0;
	uint32_t total = 0;
	bool walking = false;
	std::shared_ptr< Sound::PlayingSample > music;
	std::shared_ptr< Sound::PlayingSample > walksound_playing;
	std::shared_ptr< Sound::PlayingSample > petsound_playing;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;

	//cat info:
	struct CatInfo {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform* transform = nullptr;
	} catinfo;

	//cat transforms
	Scene::Transform* cat = nullptr;
	Scene::Transform* lbpeet = nullptr;
	Scene::Transform* lfpeet = nullptr;
	Scene::Transform* rbpeet = nullptr;
	Scene::Transform* rfpeet = nullptr;
	Scene::Transform* lear = nullptr;
	Scene::Transform* rear = nullptr;
	Scene::Transform* cattail = nullptr;
	std::array<Scene::Transform*, 8> catTransforms;

	glm::vec3 lfpeet_base;
	glm::vec3 lbpeet_base;
	glm::vec3 rbpeet_base;
	glm::vec3 rfpeet_base;
	glm::quat tail_base;
	float wobble = 0.0f;

	Scene::Transform* hand = nullptr;

	
};
