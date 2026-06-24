/*
 * Copyright (C) 2022 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

typedef struct
{
  const gchar *name;
  const gchar *email;
}
ContributorInfo;

typedef struct
{
  const gchar           *name;
  const ContributorInfo *contributors;
}
ContributorGroup;

static const ContributorInfo xfce_contributors_core[] =
{
  { "Alexander Schwinn", "alexxcons@xfce.org" },
  { "Andre Miranda", "andreldm@xfce.org" },
  { "Brian J. Tarricone", "kelnos@xfce.org" },
  { "Gaël Bonithon", "gael@xfce.org" },
  { "Olivier Fourdan", "fourdan@xfce.org" },
  { "Romain Bouvier", "skunnyk@alteroot.org" },
  { "Sergios Anestis Kefalidis", "skefalidis@xfce.org" },
  { "Simon Steinbeiß", "simon@xfce.org" },
  { "Theo Linkspfeifer", "lastonestanding@tutanota.com" },
  { NULL, NULL}
};

static const ContributorInfo xfce_contributors_active[] =
{
  { "Akbarkhon Variskhanov", "akbarkhon.variskhanov@gmail.com" },
  { "Ali Abdallah", "ali@xfce.org" },
  { "Alistair Buxton", "a.j.buxton@gmail.com" },
  { "Arkadiy Illarionov", "qarkai@gmail.com" },
  { "Erkki Moorits", "erkki.moorits@mail.ee" },
  { "Graeme Gott", "graeme@gottcode.org" },
  { "Jan Ziak", "0xe2.0x9a.0x9b@xfce.org" },
  { "Landry Breuil", "landry@xfce.org" },
  { "Sean Davis", "bluesabre@xfce.org" },
  { "Stephan Haller", "nomad@froevel.de" },
  { "Tony Paulic", "tony.paulic@gmail.com" },
  { "Yongha Hwang", "mshrimp@sogang.ac.kr" },
  { NULL, NULL }
};

static const ContributorInfo xfce_contributors_server[] =
{
  { "Romain Bouvier", "skunnyk@alteroot.org" },
  { "Mike Massonnet", "mmassonnet@gmail.com" },
  { NULL, NULL }
};

static const ContributorInfo xfce_contributors_translators_supervision[] =
{
  { "Vinzenz Vietzke", "vinz@vinzv.de" },
  { NULL, NULL }
};

static const ContributorInfo xfce_contributors_documentation_supervision[] =
{
  { "Kevin Bowen", "kevin.bowen@gmail.com" },
  { NULL, NULL }
};

static const ContributorInfo xfce_contributors_previous[] =
{
  { "Andrzej Radecki", "ndrwrdck@gmail.com" },
  { "Auke Kok", "sofar@foo-projects.org" },
  { "Benedikt Meurer", "benny@xfce.org" },
  { "Bernhard Walle", "bernhard.walle@gmx.de" },
  { "Biju Chacko", "botsie@xfce.org" },
  { "Craig Betts", "craig.betts@dfrc.nasa.gov" },
  { "Daichi Kawahata", "daichi@xfce.org" },
  { "Danny Milosavljevic", "dannym@xfce.org" },
  { "Darren Salt", "linux@youmustbejoking.demon.co.uk" },
  { "David Mohr", "squisher@xfce.org" },
  { "Edscott Wilson García", "edscott@xfce.org" },
  { "Eduard Roccatello", "eduard@xfce.org" },
  { "Ejvend Nielsen", "prophet@sphere-x.net" },
  { "Enrico Tröger", "enrico.troeger@uvena.de" },
  { "Erik Harrison", "erikharrison@xfce.org" },
  { "Eric Koegel", "eric@xfce.org" },
  { "Erik Touve", "etouve@earthlink.net" },
  { "François Le Clainche", "fleclainche@wanadoo.fr" },
  { "Guido Berhoerster", "guido+xfce@berhoerster.name" },
  { "Harald Judt", "hjudt@xfce.org" },
  { "Igor", "f2404@yandex.ru" },
  { "Jannis Pohlmann", "jannis@xfce.org" },
  { "Jasper Huijsmans", "jasper@xfce.org" },
  { "Jean-François Wauthy", "pollux@xfce.org" },
  { "Jens Guballa", "j.guballa@t-online.de" },
  { "Jens Luedicke", "jens@irs-net.com" },
  { "Joakim Andreasson", "joakim.andreasson@gmx.net" },
  { "Jérôme Guelfucci", "jeromeg@xfce.org" },
  { "Karsten Luetkewitz", "phrep@plskthx.org" },
  { "Lionel Le Folgoc", "mrpouit@gmail.com" },
  { "Martin Loschwitz", "madkiss@debian.org" },
  { "Maurizio Galli", "maurizio.galli@gmail.com" },
  { "Maximilian Schleiss", "maximilian@xfce.org" },
  { "Michael Mosier", "michael@spyonit.com" },
  { "Mickaël Graf", "korbinus@xfce.org" },
  { "Nick Schermer", "nick@xfce.org" },
  { "Olivier Duchateau", "duchateau.olivier@gmail.com" },
  { "Pau Rul·lan Ferragut", "paurullan@bulma.net" },
  { "Peter de Ridder", "peter@xfce.org" },
  { "Samuel Verstraete", "elangelo@xfce.org" },
  { "Silvio Knizek", "killermoehre@gmx.net" },
  { "Stephan Arts", "stephan@xfce.org" },
  { "Steve Dodier-Lazaro", "sidi@xfce.org" },
  { "Thomas Leonard", "tal00r@ecs.soton.ac.uk" },
  { "Tobias Henle", "tobias@page23.de" },
  { "Viktor Odintsev", "ninetls@xfce.org" },
  { "Xavier Maillard", "zedek@fxgsproject.org" },
  { "Yves-Alexis Perez", "corsac@debian.org" },
  { NULL, NULL }
};

static const ContributorGroup xfce_contributors[] =
{
  /*{ N_("Project Lead"),
    xfce_contributors_lead
  },*/
  { N_("Core developers"),
    xfce_contributors_core
  },
  { N_("Active contributors"),
    xfce_contributors_active
  },
  { N_("Servers maintained by"),
    xfce_contributors_server
  },
  { N_("Translations supervision"),
    xfce_contributors_translators_supervision
  },
  { N_("Documentation supervision"),
    xfce_contributors_documentation_supervision
  },
  { N_("Translators"),
    NULL
  },
  { N_("Previous contributors"),
    xfce_contributors_previous
  }
};
