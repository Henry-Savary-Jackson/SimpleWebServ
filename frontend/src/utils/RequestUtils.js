import axios from "axios"

axios.defaults.httpVersion = 1;
axios.defaults.decompress = true;

export async function getCSRF() {
    let response = await axios.get("/csrf", { withCredentials:true});
    axios.defaults.headers["X-CSRF-TOKEN"] = response.data;
    return response.data;
}

export async function login(username, password, csrf) {
    let formdata = new URLSearchParams();
    formdata.append("username", username);
    formdata.append("password", password);
    formdata.append("csrf", csrf);

    let response = await axios.post("/login", formdata,{ withCredentials:true});
    return response.data;
}
export async function signUp(username, password, csrf) {
    let formdata = new URLSearchParams();
    formdata.append("username", username);
    formdata.append("password", password);
    formdata.append("csrf", csrf);

    let response = await axios.post("/signup", formdata,{ withCredentials:true});
    return response.data;
}

export async function postFile (path, file_name, file_blob, csrf){
    let response = await axios.post(`${path}/${file_name}`, file_blob, {withCredentials:true})
    return response.data
}
export async function postDirectory (path, directory_name , csrf){
    let response = await axios.post(`${path}/${directory_name}`, null, {withCredentials:true})
    return response.data
}
export async function deleteFile (path ){
    let response = await axios.delete(`${path}/${file_name}`,  {withCredentials:true})
    return response.data
}
