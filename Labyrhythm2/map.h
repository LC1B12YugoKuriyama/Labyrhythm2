#pragma once

namespace Map {

	enum MAP {
		N,//noneâΩÇ‡Ç»Ç¢(ìπ)
		W,//wallï«
		G//gpalÉSÅ[Éã
	};

	const int mapSide = 32;
	const int mapNumX = 25, mapNumY = mapNumX;

	static int map[mapNumY][mapNumX] = {
		{W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W},
		{W,N,W,N,W,N,W,W,W,N,W,N,N,N,W,W,N,N,N,W,N,N,W,G,W},
		{W,N,W,N,N,N,W,N,N,N,W,N,W,N,N,N,N,W,N,N,N,W,N,N,W},
		{W,N,N,W,W,N,N,N,W,N,W,W,N,N,W,W,W,N,W,W,W,W,W,N,W},
		{W,W,N,N,N,N,W,N,W,N,N,N,N,W,N,N,N,N,N,N,N,W,N,N,W},
		{W,N,N,W,W,N,N,N,N,N,W,N,W,W,N,W,W,N,W,W,W,N,N,W,W},
		{W,N,W,N,N,N,W,N,W,W,N,W,N,N,N,N,N,N,W,N,W,N,W,N,W},
		{W,N,N,W,W,N,N,N,N,N,N,W,N,W,N,W,N,W,N,N,W,N,N,N,W},
		{W,W,N,W,N,N,W,N,W,W,N,W,N,N,W,N,N,W,W,N,W,W,N,W,W},
		{W,W,N,W,W,N,W,N,W,W,N,W,W,N,W,N,W,N,W,N,N,N,N,N,W},
		{W,N,N,N,W,N,N,N,N,N,N,N,W,W,N,N,N,N,N,W,W,N,W,N,W},
		{W,N,W,N,N,W,W,N,W,W,N,W,N,N,N,W,N,W,N,N,W,N,N,W,W},
		{W,N,N,W,N,N,W,W,N,N,W,W,N,W,N,N,N,W,N,W,W,W,N,W,W},
		{W,W,N,N,W,N,N,N,N,W,N,W,N,W,N,W,N,N,N,N,W,N,N,N,W},
		{W,N,N,W,N,N,W,W,N,N,N,W,N,N,N,N,N,W,N,W,W,N,W,N,W},
		{W,N,W,N,N,W,N,N,N,W,N,N,W,W,N,W,N,N,N,N,N,N,W,W,W},
		{W,N,W,N,W,W,W,W,N,W,W,W,W,N,N,N,N,W,N,W,N,W,N,N,W},
		{W,W,N,N,W,N,N,N,W,N,N,N,N,W,W,W,N,N,W,N,N,W,N,W,W},
		{W,N,W,N,N,N,W,N,N,N,W,W,N,N,N,W,N,W,N,N,W,N,N,N,W},
		{W,N,W,W,W,W,W,W,N,W,N,W,W,W,N,N,W,N,N,W,W,N,W,N,W},
		{W,N,N,N,N,N,W,W,N,N,N,W,N,W,W,N,N,N,W,N,N,N,N,N,W},
		{W,N,W,N,W,N,N,N,W,N,W,W,N,N,W,W,N,W,W,N,W,N,W,N,W},
		{W,N,N,N,N,N,W,N,N,N,N,W,W,N,N,N,N,N,N,N,N,N,N,N,W},
		{W,N,W,N,W,N,N,N,W,W,N,W,N,N,W,W,N,W,N,W,W,N,W,W,W},
		{W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W}
	};
}
