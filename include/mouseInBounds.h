#pragma once
#include <SFML/Graphics.hpp>
namespace inboundsCheck 
{
	inline bool mouseInBounds(const sf::RenderWindow& window)
	{
		sf::Vector2i mousePos = sf::Mouse::getPosition(window);
		bool inside = (mousePos.x >= 0 && mousePos.x < window.getSize().x &&
			mousePos.y >= 0 && mousePos.y < window.getSize().y);

		return(mousePos.x >= 0 && mousePos.x < (int)window.getSize().x &&
			mousePos.y >= 0 && mousePos.y < (int)window.getSize().y);
	}

}
