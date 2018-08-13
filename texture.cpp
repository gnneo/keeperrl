/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"
#include "texture.h"

static void checkOpenglError() {
  auto error = SDL::glGetError();
  CHECK(error == GL_NO_ERROR) << (int)error;
}

Texture::Texture() = default;

Texture::Texture(const FilePath& fileName) : path(fileName) {
  SDL::SDL_Surface* image = SDL::IMG_Load(fileName.getPath());
  CHECK(image) << SDL::IMG_GetError();
  if (auto error = loadFromMaybe(image))
    FATAL << "Couldn't load image: " << fileName << ". Error code " << toString(*error);
  SDL::SDL_FreeSurface(image);
}

Texture::Texture(const FilePath& filename, int px, int py, int w, int h) : path(filename) {
  SDL::SDL_Surface* image = SDL::IMG_Load(path->getPath());
  CHECK(image) << SDL::IMG_GetError();
  SDL::SDL_Rect offset;
  offset.x = 0;
  offset.y = 0;
  SDL::SDL_Rect src{px, py, w, h};
  SDL::SDL_Surface* sub = createSurface(src.w, src.h);
  CHECK(sub) << SDL::SDL_GetError();
  CHECK(!SDL_BlitSurface(image, &src, sub, &offset)) << SDL::SDL_GetError();
  SDL::SDL_FreeSurface(image);
  if (auto error = loadFromMaybe(sub))
    FATAL << "Couldn't load image: " << *path << ". Error code " << toString(*error);
  SDL::SDL_FreeSurface(sub);
}

Texture::Texture(Color color, int width, int height) {
  vector<Color> colors(width * height, color);
  texId = 0;
  SDL::glGenTextures(1, &*texId);
  checkOpenglError();
  SDL::glBindTexture(GL_TEXTURE_2D, *texId);
  SDL::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
  realSize = size = Vec2(width, height);
  SDL::glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(SDL::SDL_Surface* surface) {
  CHECK(!loadFromMaybe(surface));
}

Texture::Texture(Texture&& tex) {
  *this = std::move(tex);
}

Texture::~Texture() {
  if (texId)
    SDL::glDeleteTextures(1, &*texId);
}

Texture& Texture::operator=(Texture&& tex) {
  size = tex.size;
  realSize = tex.realSize;
  texId = std::move(tex.texId);
  path = tex.path;
  tex.texId = none;
  return *this;
}

optional<SDL::GLenum> Texture::loadFromMaybe(SDL::SDL_Surface* imageOrig) {
  if (!texId) {
    texId = 0;
    SDL::glGenTextures(1, &*texId);
  }
  checkOpenglError();
  SDL::glBindTexture(GL_TEXTURE_2D, *texId);
  checkOpenglError();
  SDL::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  SDL::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  SDL::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  SDL::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  checkOpenglError();
  int mode = GL_RGB;
  auto image = createPowerOfTwoSurface(imageOrig);
  if (image->format->BytesPerPixel == 4) {
    if (image->format->Rmask == 0x000000ff)
      mode = GL_RGBA;
    else
      mode = GL_BGRA;
  } else {
    if (image->format->Rmask == 0x000000ff)
      mode = GL_RGB;
    else
      mode = GL_BGR;
  }
  SDL::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  SDL::glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL::glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  SDL::glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  checkOpenglError();
  SDL::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->w, image->h, 0, mode, GL_UNSIGNED_BYTE, image->pixels);
  size = Vec2(imageOrig->w, imageOrig->h);
  realSize = Vec2(image->w, image->h);
  if (image != imageOrig)
    SDL::SDL_FreeSurface(image);
  auto error = SDL::glGetError();
  if (error != GL_NO_ERROR)
    return error;
  return none;
}

optional<Texture> Texture::loadMaybe(const FilePath& path) {
  if (SDL::SDL_Surface* image = SDL::IMG_Load(path.getPath())) {
    Texture ret;
    bool ok = !ret.loadFromMaybe(image);
    SDL::SDL_FreeSurface(image);
    if (ok) {
      ret.path = path;
      return std::move(ret);
    }
  }
  return none;
}

void Texture::addTexCoord(int x, int y) const {
  SDL::glTexCoord2f((float)x / realSize.x, (float)y / realSize.y);
}

SDL::SDL_Surface* Texture::createSurface(int w, int h) {
  SDL::SDL_Surface* ret = SDL::SDL_CreateRGBSurface(0, w, h, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
  CHECK(ret) << "Failed to creature surface " << w << ":" << h << ": " << SDL::SDL_GetError();
  return ret;
}

// Some graphic cards need power of two sized textures, so we create a larger texture to contain the original one.
SDL::SDL_Surface* Texture::createPowerOfTwoSurface(SDL::SDL_Surface* image) {
  int w = 1;
  int h = 1;
  while (w < image->w)
    w *= 2;
  while (h < image->h)
    h *= 2;
  if (w == image->w && h == image->h)
    return image;
  auto ret = createSurface(w, h);
  SDL::SDL_Rect dst{0, 0, image->w, image->h};
  SDL::SDL_SetSurfaceBlendMode(image, SDL::SDL_BLENDMODE_NONE);
  SDL_BlitSurface(image, nullptr, ret, &dst);
  // fill the rest of the texture as well, which 'kind-of' solves the problem with repeating textures.
  dst = {image->w, 0, 0, 0};
  SDL_BlitSurface(image, nullptr, ret, &dst);
  dst = {image->w, image->h, 0, 0};
  SDL_BlitSurface(image, nullptr, ret, &dst);
  dst = {0, image->h, 0, 0};
  SDL_BlitSurface(image, nullptr, ret, &dst);
  return ret;
}
