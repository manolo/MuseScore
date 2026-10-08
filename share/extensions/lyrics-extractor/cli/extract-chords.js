#!/usr/bin/env node
// Copyright (C) 2026 Manolo Carrasco (do2tis)
// SPDX-License-Identifier: GPL-3.0-or-later
var msczReader=require("../score/mscz-reader"),xmlExtractor=require("../score/xml-extractor"),filePath=process.argv[2];filePath||(process.stderr.write("Usage: node extract-chords.js <score.mscz|score.mscx>\n"),process.exit(1));try{var xmlString=msczReader.readScore(filePath),excerptXmls=[];if(filePath.match(/\.mscz$/i))try{var excerpts=msczReader.readGuitarExcerpts(filePath);excerptXmls=excerpts.map(function(e){return e.xml})}catch(e){}var data=xmlExtractor.extractAll(xmlString,excerptXmls);process.stdout.write(JSON.stringify({chords:data.chords,fretDiagrams:data.fretDiagrams||[]}))}catch(e){process.stderr.write("Error: "+e.message+"\n"),process.exit(1)}
