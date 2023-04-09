// Copyright 2022-2023 The MathWorks, Inc.
json=require('./links.json');
let name=process.env.THIRD_PARTY_NAME;
let version=process.env.THIRD_PARTY_VERSION
let arch=process.env.ARCH;
let link;
for(let index in json.files)
{
    let obj =json.files[index];
    if(obj.name===name && obj.version==version && obj.arch===arch)
    {
        link=obj["downloadLink"];
        break;
    }
}
console.log(link);