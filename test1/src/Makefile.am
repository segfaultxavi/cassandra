bin_PROGRAMS = test1Performance test1

TESTS = test1Performance test1

test1Performance_SOURCES = Game1.cpp CassandraTest1Performance.cpp
test1Performance_CXXFLAGS = -I$(top_srcdir)/lib/src/
test1Performance_LDADD = $(top_srcdir)/lib/src/libcassandra.a

test1_SOURCES = Game1.cpp CassandraTest1.cpp glew.c
test1_CXXFLAGS = -I$(top_srcdir)/lib/src/ -I$(top_srcdir)/contrib/glew-1.11.0/include $(SDL_CFLAGS) $(GL_CFLAGS)
test1_CFLAGS = -I$(top_srcdir)/lib/src/ -I$(top_srcdir)/contrib/glew-1.11.0/include $(SDL_CFLAGS) $(GL_CFLAGS)
test1_LDADD = $(top_srcdir)/lib/src/libcassandra.a $(SDL_LIBS) $(GL_LIBS)
