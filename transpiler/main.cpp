/*
 *  vpy2cpp - command line
 *
 *  usage: vpy2cpp program.json > program.cpp
 *         python3 vpy_ast.py program.py | vpy2cpp - > program.cpp
 */

import std;
import vpy2cpp;

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::println(std::cerr, "usage: vpy2cpp <program.json | ->");
    return 2;
  }
  const std::string path = argv[1];
  std::string text;
  if (path == "-")
    text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
  else
  {
    std::ifstream in(path);
    if (!in)
    {
      std::println(std::cerr, "vpy2cpp: can't read {}", path);
      return 1;
    }
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  try
  {
    std::print("{}", vpy2cpp::translate(text));
  }
  catch (const vpy2cpp::translate_error& e)
  {
    std::println(std::cerr, "{}:{}: {}", path, e.line, e.what());
    return 1;
  }
  catch (const std::exception& e)
  {
    std::println(std::cerr, "vpy2cpp: {}", e.what());
    return 1;
  }
  return 0;
}
