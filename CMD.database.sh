#!/bin/bash
cd SQL_scripts
mysql -u root < BD_Main_Create.sql
mysql -u root < BD_Main_Insert.sql
