#include <iostream>
#include "game_message.pb.h"
using namespace std;

int main()
{
	game::Player _player;
	_player.set_name("John Doe");
	std::cout << "Player Name: " << _player.name() << std::endl;

	game::Item _item;
	_item.set_name("bow");
	_item.set_value(1);
}