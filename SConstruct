env = Environment()
if env['PLATFORM'] == 'darwin':
   env['CXX']='clang++'
   env.Append(CXXFLAGS=['-O3', '-g', '-std=c++11', '-Wall', '-Wextra', '-pedantic', '-stdlib=libc++'])
   env.Append(LINKFLAGS=['-stdlib=libc++'])
else:
   env['CXX']='g++-4.6'
   env.Append(CXXFLAGS=['-O3', '-g', '-std=c++0x', '-Wall', '-Wextra', '-pedantic', '-pthread'])
   env.Append(LINKFLAGS=['-pthread'])

env.Program(source=['main.cpp', 'block.cpp'], LIBS=['perfmon'])
