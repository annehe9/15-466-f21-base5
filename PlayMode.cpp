#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "Sound.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>

#include <random>
#include <time.h>

GLuint city_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > city_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("city.pnct"));
	city_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > city_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("city.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = city_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = city_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > city_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("city.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

Load< Sound::Sample > walksound(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("walking_sound.wav"));
	});

Load< Sound::Sample > petsound(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("pet.wav"));
	});

PlayMode::PlayMode() : scene(*city_scene) {
	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 1.3f); //hand at 1.1

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);

	//get pointers:
	for (auto& drawable : scene.drawables) {
		//sadness that switch doesnt work on string
		if (drawable.transform->name == "Hand") {
			hand = drawable.transform;
			hand->position = (player.camera->transform->make_world_to_local()) * glm::vec4(0.4, 1.2f, 1.0f, 1.0f);
			// We should be inverting the parent rotation here. Not sure how to though
			hand->rotation = glm::inverse(player.camera->transform->rotation)* hand->rotation;
			hand->rotation = glm::rotate(hand->rotation, glm::radians(-70.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			hand->rotation = glm::rotate(hand->rotation, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			hand->parent = player.camera->transform;
			//hand->parent = player.camera->transform;
			//hand->position = glm::vec3(1.0f, 0.0f, -0.2f);
		}
		else if (drawable.transform->name == "Left Back Foot") {
			lbpeet = drawable.transform;
		}
		else if (drawable.transform->name == "Left Ear") {
			lear = drawable.transform;
		}
		else if (drawable.transform->name == "Left Front Foot") {
			lfpeet = drawable.transform;
		}
		else if (drawable.transform->name == "Right Back Foot") {
			rbpeet = drawable.transform;
		}
		else if (drawable.transform->name == "Right Ear") {
			rear = drawable.transform;
		}
		else if (drawable.transform->name == "Right Front Foot") {
			rfpeet = drawable.transform;
		}
		else if (drawable.transform->name == "Sphere") {
			cat = drawable.transform;
		}
		else if (drawable.transform->name == "Tail") {
			cattail = drawable.transform;
		}
	}

	catTransforms = { cat, lbpeet, lfpeet, rbpeet, rfpeet, lear, rear, cattail };

	// Make all cat parts children of the cat body
	for (Scene::Transform* transform : catTransforms) {
		if (transform != cat) {
			transform->position = (cat->make_world_to_local()) * glm::vec4(transform->position.x, transform->position.y, transform->position.z, 1.0f);
			// We should be inverting the parent rotation here. Not sure how to though
			transform->rotation = glm::inverse(cat->rotation) * transform->rotation;
			transform->parent = cat;
		}
	}
	lfpeet_base = lfpeet->position;
	lbpeet_base = lbpeet->position;
	rfpeet_base = rfpeet->position;
	rbpeet_base = rbpeet->position;

	lfpeet_base = lfpeet->position;
	lbpeet_base = lbpeet->position;
	rfpeet_base = rfpeet->position;
	rbpeet_base = rbpeet->position;
	tail_base = cattail->rotation;

	//start music:
	music = Sound::play(*(new Sound::Sample(data_path("ancientwinds.opus"))), 1.0f, 1.0f);

	srand((unsigned)time(NULL) + 15466);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			if (!walking) {
				walksound_playing = Sound::play(*walksound, 0.5f, 1.0f);
				walking = true;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			if (!walking) {
				walksound_playing = Sound::play(*walksound, 0.5f, 1.0f);
				walking = true;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			if (!walking) {
				walksound_playing = Sound::play(*walksound, 0.5f, 1.0f);
				walking = true;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			if (!walking) {
				walksound_playing = Sound::play(*walksound, 0.5f, 1.0f);
				walking = true;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = true;
			hand->position = glm::vec3(hand->position.x, hand->position.y-0.3, hand->position.z);
			if (glm::distance(player.transform->position,cat->position) <= 2.0f) {
				space.downs += 1;
				Sound::play(*petsound, 0.5f, 1.0f);
			}
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			walking = false;
			walksound_playing.get()->stop();
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			walking = false;
			walksound_playing.get()->stop();
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			walking = false;
			walksound_playing.get()->stop();
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			walking = false;
			walksound_playing.get()->stop();
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			hand->position = glm::vec3(hand->position.x, hand->position.y + 0.3, hand->position.z);
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 up = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, up) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
			//hand->rotation = player.camera->transform->rotation;

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//player walking:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}
		//hand->position = (player.camera->transform->make_world_to_local()) * glm::vec4(0.4, 1.2f, 1.0f, 1.0f);

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/

		//animation
		//slowly rotates through [0,1):
		wobble += elapsed / 20.0f;
		//wobble -= std::floor(wobble);
		float tail_angle = 20.f * std::sin(wobble * 10.0f * 2.0f * float(M_PI));
		cattail->rotation = tail_base * glm::angleAxis(tail_angle * DEG2RAD, glm::vec3(0.f, 1.f, 0.f));
	}

	//add to score
	score += space.downs;
	total += space.downs;
	if (score >= 10) {
		float randx = (20.0f * (float)rand() / RAND_MAX) - 10;
		float randy = (20.0f * (float)rand() / RAND_MAX) - 10;

		glm::vec3 old_pos = cat->position;
		cat->position = glm::vec3(cat->position.x + randx, cat->position.y + randy, cat->position.z);
		WalkPoint cat_at = walkmesh->nearest_walk_point(cat->position);
		cat->position = walkmesh->to_world_point(cat_at);
		cat->position = glm::vec3(cat->position.x, cat->position.y, old_pos.z);
		/*glm::vec3 delta = cat->position - old_pos;
		cat->position = old_pos;
		for (Scene::Transform* c : catTransforms) {
			c->position += delta;
		}*/
		score = 0;
	}

	//debug code that helps you find the cat
	std::cout << "you are at " << player.transform->position.x << ", " << player.transform->position.y << ", " << player.transform->position.z << "\n";
	std::cout << "cat is at " << cat->position.x << ", " << cat->position.y << ", " << cat->position.z << "\n";

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse, Space to pet",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse, Space to pet",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		lines.draw_text("You have pet the cat " + std::to_string((int)total) + " times.",
			glm::vec3(-aspect + 0.1f * H, 1.0f - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("You have pet the cat " + std::to_string((int)total) + " times.",
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0f - 1.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
