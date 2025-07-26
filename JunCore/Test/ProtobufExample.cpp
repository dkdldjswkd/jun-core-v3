#include "ProtobufExample.h"
#include "game_message.pb.h"

void ProtobufExample() 
{
	// Protobuf example
	game::Player _player;
	_player.set_name("John Doe");
	std::cout << "Player Name: " << _player.name() << std::endl;

	game::Item _item;
	_item.set_value(1);
	std::cout << "Item Value: " << _item.value() << std::endl;
}